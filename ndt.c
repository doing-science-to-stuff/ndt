/*
 * ndt.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2014-2019 Bryan Franklin. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <dlfcn.h>
#ifdef WITH_VALGRIND
#include <valgrind/valgrind.h>
#endif /* WITH_VALGRIND */
#ifdef WITH_MPI
#include <mpi.h>
#endif /* WITH_MPI */
#include "vectNd.h"
#include "image.h"
#include "object.h"
#include "scene.h"
#include "timing.h"

#ifndef MIN
#define MIN(x,y) (((x)>(y))?(y):(x))
#endif /* MIN */
#ifndef MAX
#define MAX(x,y) (((x)<(y))?(y):(x))
#endif /* MAX */

#if 1
#define WITH_SPECULAR
#endif /* 0 */

#ifdef WITH_SPECULAR
int specular_enabled = 1;
#endif /* WITH_SPECULAR */

int recursive_aa = 0;

typedef enum stereo_mode_t {
    MONO, SIDE_SIDE_3D, OVER_UNDER_3D, ANAGLYPH_3D, HIDEF_3D
} stereo_mode;
    
#ifdef WITH_MPI
typedef enum mpi_mode {
    MPI_MODE_NONE,
    MPI_MODE_PIXEL, /* not supported, yet! */
    MPI_MODE_ROW,
    MPI_MODE_FRAME,     /* rank 0 just manages other ranks */
    MPI_MODE_FRAME2,    /* rank 0 also renders frames */
} mpi_mode_t;

static int mpiRank = -1;
static int mpiSize = -1;
static mpi_mode_t mpi_mode = MPI_MODE_NONE;

int mpi_collect_image(image_t*);
#endif /* WITH_MPI */

static inline int apply_lights(scene *scn, int dim, object *obj_ptr, vectNd *src, vectNd *look, vectNd *hit, vectNd *hit_normal, dbl_pixel_t *color) {
    dbl_pixel_t clr;
    double hit_r, hit_g, hit_b;
    vectNd rev_view, rev_light, light_vec, light_hit, light_hit_normal;
    vectNd lgt_pos, near_pos;

    /* get color of object */
    obj_ptr->get_color(obj_ptr, hit, &hit_r, &hit_g, &hit_b);

    #ifdef WITH_SPECULAR
    double hitr_r = 0.0, hitr_g = 0.0, hitr_b = 0.0;
    if( specular_enabled ) {
        /* get reflectivity of object */
        obj_ptr->get_reflect(obj_ptr, hit, &hitr_r, &hitr_g, &hitr_b);
    }
    #endif /* WITH_SPECULAR */

    memset(&clr,'\0',sizeof(clr));
    clr.r = hit_r * scn->ambient.red;
    clr.g = hit_g * scn->ambient.green;
    clr.b = hit_b * scn->ambient.blue;

    /* all of these are initialized within the loop */
    vectNd_alloc(&rev_view,dim);
    vectNd_alloc(&rev_light,dim);
    vectNd_alloc(&light_vec,dim);
    vectNd_alloc(&light_hit,dim);
    vectNd_alloc(&light_hit_normal,dim);
    vectNd_alloc(&lgt_pos,dim);
    vectNd_alloc(&near_pos,dim);
    int i=0;
    for(i=0; i<scn->num_lights; ++i) {

        light_type lgt_type = scn->lights[i]->type;
        if( lgt_type == LIGHT_AMBIENT ) {
            clr.r += hit_r * scn->lights[i]->red;
            clr.g += hit_g * scn->lights[i]->green;
            clr.b += hit_b * scn->lights[i]->blue;
            continue;
        }

        /* copy location of actual light */
        vectNd_copy(&lgt_pos,&scn->lights[i]->pos);

        if( lgt_type == LIGHT_DISK ||
            lgt_type == LIGHT_RECT ) {
            /* move light to a random point on areal light */
            double x,y;
            vectNd temp;
            vectNd_alloc(&temp,scn->lights[i]->pos.n);

            if( scn->lights[i]->prepared == 0 ) {
                scene_prepare_light(scn->lights[i]);
            }

            double radius = scn->lights[i]->radius;
            /* get one random sample */
            /* re-sampling happens at the pixel level */
            do {
                x = 2 * drand48() - 1.0;
                y = 2 * drand48() - 1.0;
                /* reject any samples outside unit circle */
                /* see: Ray Tracing From The Ground Up, p. 120 */
            } while (lgt_type == LIGHT_DISK && x*x + y*y > 1.0 );

            /* map point onto light surface */
            vectNd_scale(&scn->lights[i]->u1,x*radius,&temp);
            vectNd_add(&lgt_pos,&temp,&lgt_pos);
            vectNd_scale(&scn->lights[i]->v1,y*radius,&temp);
            vectNd_add(&lgt_pos,&temp,&lgt_pos);

            /* treat sampled point as a regular point light source */
            lgt_type = LIGHT_POINT;

            vectNd_free(&temp);
        }

        if( lgt_type == LIGHT_POINT ||
            lgt_type == LIGHT_DIRECTIONAL ||
            lgt_type == LIGHT_SPOT ) {
            /* determine if light is in correct direction */
            if( lgt_type == LIGHT_POINT ||
                lgt_type == LIGHT_SPOT ) {
                vectNd_sub(&lgt_pos, hit, &rev_light);
            } else if( scn->lights[i]->type == LIGHT_DIRECTIONAL ) {
                vectNd_scale(&scn->lights[i]->dir, -1, &rev_light);
            }
            vectNd_sub(src, hit, &rev_view);
            double dotRev1, dotRev2;
            vectNd_dot(&rev_light, hit_normal,&dotRev1);
            vectNd_dot(&rev_view, hit_normal,&dotRev2);
            if( (dotRev1*dotRev2) <= 0 ) {
                /* light and observer are on opposite sides of the surface */
                /*printf("light on the wrong side of surface\n");*/
                continue;
            }
        }

        if( lgt_type == LIGHT_POINT ||
            lgt_type == LIGHT_SPOT ||
            lgt_type == LIGHT_DIRECTIONAL ) {
            object *light_obj_ptr=NULL;
            int got_hit = 0;

            double ldist2=1.0;
            if( lgt_type == LIGHT_POINT ||
                lgt_type == LIGHT_SPOT ) {
                vectNd_sub(hit, &lgt_pos, &light_vec);

                /* check that hit point is within cone of light for
                 * spotlight, skip tracing path if not */
                if( lgt_type == LIGHT_SPOT ) {
                    double angle = -1;
                    vectNd_angle(&scn->lights[i]->dir,&light_vec,&angle);
                    if( (angle*180.0/M_PI) > scn->lights[i]->angle ) {
                        continue;
                    }
                }

                /* trace from light to object */
                got_hit = trace(&lgt_pos, &light_vec, scn->object_ptrs, scn->num_objects,
                    &light_hit, &light_hit_normal, &light_obj_ptr);
                if( !got_hit || light_obj_ptr != obj_ptr ) {
                    /* light didn't hit the target object */
                    continue;
                }

                double dist = 1000;
                vectNd_dist(hit,&light_hit,&dist);
                /* verify we hit the right point with the light */
                if( dist > EPSILON ) {
                    /* light didn't hit the same point on the object */
                    continue;
                }

                /* distance for diffuse lighting */
                vectNd_dot(&light_vec,&light_vec,&ldist2);
            } else if( lgt_type == LIGHT_DIRECTIONAL ) {
                /* trace from object towards light */
                vectNd_copy(&near_pos,&scn->lights[i]->dir);
                vectNd_unitize(&near_pos);
                vectNd_scale(&near_pos,-EPSILON,&near_pos);
                vectNd_add(&near_pos,hit,&near_pos);
                vectNd_scale(&scn->lights[i]->dir, -1.0, &light_vec);
                got_hit = trace(&near_pos, &rev_light, scn->object_ptrs, scn->num_objects,
                    &light_hit, &light_hit_normal, &light_obj_ptr);

                /* success is not hitting anything */
                if( got_hit ) {
                    /* something between object and infinity */
                    continue;
                }

                /* prep vectors for remainder of computations */
                vectNd_copy(&light_vec, &scn->lights[i]->dir);
                vectNd_copy(&light_hit, hit);
                vectNd_copy(&light_hit_normal, hit_normal);
                light_obj_ptr = obj_ptr;

                /* distance is irrelevant for diffuse lighting */
                ldist2 = 1;
            }

            double angle = -1;
            double light_scale = 1;
            vectNd_angle(hit_normal,&light_vec,&angle);

            /* diffuse lighting */
            if( angle > M_PI / 2.0 )
                angle = M_PI - angle;
            light_scale = cos(angle) / ldist2;
            if( !obj_ptr->transparent ) {
                clr.r += hit_r * scn->lights[i]->red * light_scale;
                clr.g += hit_g * scn->lights[i]->green * light_scale;
                clr.b += hit_b * scn->lights[i]->blue * light_scale;
            }
        }

        #ifdef WITH_SPECULAR
        if( lgt_type == LIGHT_POINT ||
            lgt_type == LIGHT_SPOT ||
            lgt_type == LIGHT_DIRECTIONAL ) {
            if( specular_enabled ) {
                /* specular highlighting */
                /* see: http://en.wikipedia.org/wiki/Specular_highlight */
                /* note: wikipedia doesn't mention that these have to be unit vectors */
                /* slide 28 of
                 * http://www.eng.utah.edu/~cs5600/slides/Wk%2013%20Ray%20Tracing.pdf
                 * looks interesting. */
                vectNd light_ref;
                vectNd_alloc(&light_ref,dim);
                vectNd_reflect(&light_vec,&light_hit_normal,&light_ref,0.5);

                double rv;
                vectNd rev_look;
                vectNd_alloc(&rev_look,look->n);
                vectNd_unitize(&light_ref);
                vectNd_scale(look,-1,&rev_look);
                vectNd_unitize(&rev_look);
                vectNd_dot(&light_ref,&rev_look,&rv);
                vectNd_free(&rev_look);
                rv = MAX(0,rv);
                double rvn = pow(rv,50);

                double max_light = MAX(scn->lights[i]->red, MAX(scn->lights[i]->green, scn->lights[i]->blue));
                clr.r += hitr_r * scn->lights[i]->red/max_light * rvn;
                clr.g += hitr_g * scn->lights[i]->green/max_light * rvn;
                clr.b += hitr_b * scn->lights[i]->blue/max_light * rvn;

                vectNd_free(&light_ref);
            }
        }
        #else
        #warning "Specular highlighting not enabled."
        #endif /* WITH_SPECULAR */
    }
    vectNd_free(&lgt_pos);
    vectNd_free(&near_pos);
    vectNd_free(&light_vec);
    vectNd_free(&light_hit);
    vectNd_free(&light_hit_normal);
    vectNd_free(&rev_light);
    vectNd_free(&rev_view);

    memcpy(color,&clr,sizeof(clr));

    return 0;
}
    
/* get color of ray r,g,b \in [0,1] */
int get_ray_color(vectNd *src, vectNd *look, scene *scn, dbl_pixel_t *pixel,
            double pixel_frac, double *depth, int max_depth)
{
    int ret = 0;

    pixel->r = pixel->g = pixel->b = 0.0;
    if( pixel_frac < (1.0/512.0) )
        return 1;
    //printf("pixel_frac = %g\n", pixel_frac);

    if( max_depth <= 0 )
        return 1;

    vectNd hit;
    vectNd hit_normal;
    double hitr_r, hitr_g, hitr_b;
    object *obj_ptr=NULL;
    int dim = src->n;
    dbl_pixel_t clr;

    memset(&clr,'\0',sizeof(clr));

    /* trace from camera to possible object */
    vectNd_calloc(&hit,dim);
    vectNd_calloc(&hit_normal,dim);

    obj_ptr = NULL;
    trace(src, look, scn->object_ptrs, scn->num_objects, &hit, &hit_normal, &obj_ptr);

    /* record depth for depth maps */
    double trace_dist = -1;
    if( obj_ptr != NULL || depth != NULL ) {
        vectNd_dist(&hit,src,&trace_dist);
        if( depth != NULL ) {
            /* record distance to hit */
            if( trace_dist > EPSILON )
                *depth = 1.0/trace_dist;
        }
    }
    if( obj_ptr == NULL && depth != NULL )
        *depth = 0.0;

    /* apply light */
    if( obj_ptr != NULL && trace_dist > EPSILON )
    {
        apply_lights(scn,dim,obj_ptr,src,look,&hit,&hit_normal,&clr);

        /* get reflectivity of object */
        obj_ptr->get_reflect(obj_ptr, &hit, &hitr_r, &hitr_g, &hitr_b);

        #if 1
        /* compute reflection */
        /* see:
         * http://www.unc.edu/~marzuola/Math547_S13/Math547_S13_Projects/P_Smith_Section001_RayTracing.pdf
         */
        double contrib = MAX(hitr_r,MAX(hitr_g,hitr_b));
        if(contrib > 0 ) {
            vectNd reflect;
            vectNd_alloc(&reflect,dim);
            vectNd_reflect(look,&hit_normal,&reflect,1.0);

            /* set color based on actual reflection */
            dbl_pixel_t ref;
            get_ray_color(&hit,&reflect,scn,&ref, contrib*pixel_frac, NULL, max_depth-1);
            #ifdef WITH_SPECULAR
            if( specular_enabled ) {
                clr.r = (1-hitr_r)*(clr.r)+(hitr_r)*ref.r;
                clr.g = (1-hitr_g)*(clr.g)+(hitr_g)*ref.g;
                clr.b = (1-hitr_b)*(clr.b)+(hitr_b)*ref.b;
            } else {
            #endif /* WITH_SPECULAR */
                clr.r += hitr_r*ref.r;
                clr.g += hitr_g*ref.g;
                clr.b += hitr_b*ref.b;
            #ifdef WITH_SPECULAR
            }
            #endif /* WITH_SPECULAR */

            vectNd_free(&reflect);
        }
        #endif /* 1 */

        ret = 1;
    } else {
        /* didn't hit an object, so use background color */
        clr.r = scn->bg_red;
        clr.g = scn->bg_green;
        clr.b = scn->bg_blue;
        ret = 0;
    }

    memcpy(pixel,&clr,sizeof(clr));
    vectNd_free(&hit);
    vectNd_free(&hit_normal);

    return ret;
}

typedef enum camera_mode_t {
    CAM_LEFT, CAM_CENTER, CAM_RIGHT
} camera_mode;

int get_pixel_color(scene *scn, int width, int height, double x, double y,
    dbl_pixel_t *clr, int samples, camera_mode mode, double *depth,
    int max_optic_depth)
{
    vectNd look;
    vectNd pixel;
    vectNd virtCam;
    vectNd temp;
    int dim = scn->cam.pos.n;

    /* compute look vector */
    /* pixelPos = cam.imgOrig + x*cam.dirX + y*cam.dirY */
    /* look = pixelPos - cam.pos */
    vectNd_alloc(&pixel,dim);
    vectNd_alloc(&virtCam,dim);
    vectNd_alloc(&look,dim);
    vectNd_alloc(&temp,dim);

    int min_samples = samples;
    int max_samples = 10000;
    double max_diff = 1.0/256.0;
    double clr_diff = 256;

    dbl_pixel_t t_clr;
    t_clr.r = t_clr.g = t_clr.b = 0.0;
    int t_samples = 0;
    int i=0;
    double pixel_width = 1.0/width;
    double pixel_height = 1.0/height;
    double orig_x, orig_y;
    orig_x = x;
    orig_y = y;
    for(i=0; i<min_samples || (i<max_samples && clr_diff > max_diff); ++i) {
        dbl_pixel_t l_clr;

        switch(mode) {
            case CAM_LEFT:
                vectNd_copy(&virtCam,&scn->cam.leftEye);
                break;
            case CAM_RIGHT:
                vectNd_copy(&virtCam,&scn->cam.rightEye);
                break;
            case CAM_CENTER:
            default:
                vectNd_copy(&virtCam,&scn->cam.pos);
                break;
        }

        /* apply pixel sampling for anti-aliasing */
        if( recursive_aa == 0 && samples > 1 ) {
            double dx,dy;
            /* perturb look vector slightly */
            /* see: Ray Tracing From The Ground Up, p. 172 */
            dx = drand48();
            dy = drand48();

            x = orig_x + dx * pixel_width;
            y = orig_y + dy * pixel_height;
        }

        double focal_dist = scn->cam.focal_distance;
        camera_target_point(&scn->cam, x, y, focal_dist, &pixel);

        if( scn->cam.type == CAMERA_VR || scn->cam.type == CAMERA_PANO ) {
            /* For VR, rotate camera around CAM_CENTER point */
            if( mode != CAM_CENTER ) {
                double azi = x * scn->cam.hFov;
                vectNd_rotate2(&virtCam,&scn->cam.pos,&scn->cam.localX,&scn->cam.localZ,azi,&virtCam);
            }
        }

        /* apply aperture sampling for depth of field */
        if( samples > 1 ) {
            double x,y;
            /* perturb look vector slightly */
            /* see: Ray Tracing From The Ground Up, p. 171 */
            do {
                x = 2 * drand48() - 1.0;
                y = 2 * drand48() - 1.0;
                /* reject any samples outside unit circle */
                /* see: Ray Tracing From The Ground Up, p. 120 */
            } while (x*x + y*y > 1.0 );
            vectNd_scale(&scn->cam.localX, x*scn->cam.aperture_radius, &temp);
            vectNd_add(&virtCam, &temp, &virtCam);
            vectNd_scale(&scn->cam.localY, y*scn->cam.aperture_radius, &temp);
            vectNd_add(&virtCam, &temp, &virtCam);
        }

        /* compute primary ray to use */
        vectNd_sub(&pixel, &virtCam, &look);

        l_clr.r = l_clr.g = l_clr.b = 0.0;
        get_ray_color(&virtCam, &look, scn, &l_clr, 1.0, depth, max_optic_depth);

        if( i > 1 ) {
            clr_diff = MAX( fabs(t_clr.r / (i-1) - (t_clr.r+l_clr.r) / i),
                        MAX( fabs(t_clr.g / (i-1) - (t_clr.g+l_clr.g) / i),
                             fabs(t_clr.b / (i-1) - (t_clr.b+l_clr.b) / i) ) );
        }

        t_clr.r += l_clr.r;
        t_clr.g += l_clr.g;
        t_clr.b += l_clr.b;
        t_samples += 1;
    }

    clr->r = t_clr.r/t_samples;
    clr->g = t_clr.g/t_samples;
    clr->b = t_clr.b/t_samples;

    vectNd_free(&temp);
    vectNd_free(&look);
    vectNd_free(&pixel);
    vectNd_free(&virtCam);

    return 1;
}

int render_pixel(scene *scn, int width, double x_scale, int height, double y_scale, double i, double j, stereo_mode mode, int samples, dbl_pixel_t *clr, double *depth, int max_optic_depth)
{
    double ip = 0;
    double jp = 0;
    camera_mode cam_mode = CAM_CENTER;

    double x = 0.0;
    double y = 0.0;

    ip = i;
    jp = j;
    cam_mode = CAM_CENTER;
    if( mode == SIDE_SIDE_3D ) {
        if( i < width/2 ) {
            // left image
            ip = ip/x_scale;
            cam_mode = CAM_LEFT;
        } else {
            // right image
            ip = (ip-width/2)/x_scale;
            cam_mode = CAM_RIGHT;
        }
    }
    if( mode == OVER_UNDER_3D ) {
        if( j < height/2 ) {
            // top image
            jp = jp/y_scale;
            cam_mode = CAM_LEFT;
        } else {
            // bottom image
            jp = (jp-height/2)/y_scale;
            cam_mode = CAM_RIGHT;
        }
    }

    if( mode == HIDEF_3D ) {
        if( j < 1080 ) {
            // left image
            cam_mode = CAM_LEFT;
        } else if( j > (1080+45) ) {
            // bottom image
            jp = j - (1080+45);
            cam_mode = CAM_RIGHT;
        } else {
            // some wierd blanking thing
            // see:
            // http://www.tomshardware.com/reviews/blu-ray-3d-3d-video-3d-tv,2632-6.html
            clr->r = clr->g = clr->b = 0;
            return 0;
        }

        x = ip/(double)width - 0.5;
        y = -(jp/1080.0 - 0.5);
    } else {
        x = ip/(double)width - 0.5;
        y = -(jp/(double)height - 0.5);
    }

    if( mode == ANAGLYPH_3D ) {
        dbl_pixel_t left_clr;
        dbl_pixel_t right_clr;

        get_pixel_color(scn, width, height, x, y, &left_clr, samples, CAM_LEFT, depth, max_optic_depth);
        get_pixel_color(scn, width, height, x, y, &right_clr, samples, CAM_RIGHT, NULL, max_optic_depth);

        /* true anaglyph */
        clr->r = 0.299*left_clr.r+0.587*left_clr.g+0.114*left_clr.b;
        clr->g = 0;
        clr->b = 0.299*right_clr.r+0.587*right_clr.g+0.114*right_clr.b;
    } else {
        get_pixel_color(scn, width, height, x, y, clr, samples, cam_mode, depth, max_optic_depth);
    }
    clr->a = 1.0;

    return 0;
}

int recursive_resample(scene *scn, int width, double x_scale,
        int height, double y_scale, double x, double y,
        int samples, int aa_diff, int aa_depth, stereo_mode mode, double step,
        dbl_pixel_t *p1, dbl_pixel_t *p2, dbl_pixel_t *p3, dbl_pixel_t *p4,
        dbl_pixel_t *res, int max_optic_depth)
{
    dbl_pixel_t p5, p6, p7, p8, p9;

    if( aa_depth<=0 || step < 1.0/(2<<(aa_depth-1)) ) {
        image_avg_dbl_pixels4(p1,p2,p3,p4,res,NULL);
        return 0;
    }

    double hs=step/2;
    /* center */
    render_pixel(scn, width, x_scale, height, y_scale, x+hs, y+hs, mode, samples, &p5, NULL, max_optic_depth);
    /* top middle */
    render_pixel(scn, width, x_scale, height, y_scale, x+hs, y, mode, samples, &p6, NULL, max_optic_depth);
    /* left edge */
    render_pixel(scn, width, x_scale, height, y_scale, x, y+hs, mode, samples, &p7, NULL, max_optic_depth);
    /* right edge */
    render_pixel(scn, width, x_scale, height, y_scale, x+step, y+hs, mode, samples, &p8, NULL, max_optic_depth);
    /* bottom middle */
    render_pixel(scn, width, x_scale, height, y_scale, x+hs, y+step, mode, samples, &p9, NULL, max_optic_depth);

    /* compute 4 sub-pixels */
    dbl_pixel_t sp1, sp2, sp3, sp4;
    double var1=0, var2=0, var3=0, var4=0, var5;
    double threshold = aa_diff/255.0;
    /* top left */
    image_avg_dbl_pixels4(p1,&p6,&p7,&p5,&sp1,&var1);
    if( var1 > threshold )
        recursive_resample(scn,width,x_scale,height,y_scale,x,y,samples,aa_diff,aa_depth,mode,hs,p1,&p6,&p7,&p5,&sp1,max_optic_depth);

    /* top right */
    image_avg_dbl_pixels4(p2,&p6,&p8,&p5,&sp2,&var2);
    if( var2 > threshold )
        recursive_resample(scn,width,x_scale,height,y_scale,x+hs,y,samples,aa_diff,aa_depth,mode,hs,&p6,p2,&p5,&p8,&sp2,max_optic_depth);

    /* bottom left */
    image_avg_dbl_pixels4(p3,&p9,&p7,&p5,&sp3,&var3);
    if( var3 > threshold )
        recursive_resample(scn,width,x_scale,height,y_scale,x,y+hs,samples,aa_diff,aa_depth,mode,hs,&p7,&p5,p3,&p9,&sp3,max_optic_depth);

    /* bottom right */
    image_avg_dbl_pixels4(p4,&p9,&p8,&p5,&sp4,&var4);
    if( var4 > threshold )
        recursive_resample(scn,width,x_scale,height,y_scale,x+hs,y+hs,samples,aa_diff,aa_depth,mode,hs,&p5,&p8,&p9,p4,&sp4,max_optic_depth);
    
    image_avg_dbl_pixels4(&sp1,&sp2,&sp3,&sp4,res,&var5);

    return var5;
}

int resample_pixel(scene *scn, int width, double x_scale, int height, double y_scale, int i, int j, stereo_mode mode, int samples, int aa_diff, int aa_depth, image_t *img, image_t *prev_raw, image_t *prev_aa, dbl_pixel_t *clr, int max_optic_depth) {
    dbl_pixel_t p1, p2, p3, p4;
    double var;
    int ret = 0;

    dbl_image_get_pixel(img,i,j,&p1);
    dbl_image_get_pixel(img,i+1,j,&p2);
    dbl_image_get_pixel(img,i,j+1,&p3);
    dbl_image_get_pixel(img,i+1,j+1,&p4);

    if( prev_raw != NULL && prev_aa !=NULL ) {
        dbl_pixel_t pp1, pp2, pp3, pp4;   
        dbl_image_get_pixel(prev_raw,i,j,&pp1);
        dbl_image_get_pixel(prev_raw,i+1,j,&pp2);
        dbl_image_get_pixel(prev_raw,i,j+1,&pp3);
        dbl_image_get_pixel(prev_raw,i+1,j+1,&pp4);
        if( memcmp(&pp1,&p1,sizeof(pixel_t))==0 &&
                memcmp(&pp2,&p2,sizeof(pixel_t))==0 &&
                memcmp(&pp3,&p3,sizeof(pixel_t))==0 &&
                memcmp(&pp4,&p4,sizeof(pixel_t))==0 ) {
            dbl_image_get_pixel(prev_aa,i,j,clr);
            return 0;
        }
    }

    image_avg_dbl_pixels4(&p1,&p2,&p3,&p4,clr,&var);

    /* resample pixel if needed */
    if( var > aa_diff/255.0 ) {
        ret = 1;
        recursive_resample(scn,width+1,x_scale,height+1,y_scale,
                i,j,samples,aa_diff,aa_depth,mode,1.0, &p1, &p2, &p3, &p4, clr, max_optic_depth);
    }

    return ret;
}

int render_line(scene *scn, int width, double x_scale, int height, double y_scale, int j, stereo_mode mode, int samples, image_t *img, image_t *depth_map, int max_optic_depth)
{
    dbl_pixel_t clr;
    dbl_pixel_t depth_clr;
    double depth;
    int i=0;
    depth_clr.a = 1.0;
    int row_start = 0;
    int row_step = 1;
    #ifdef WITH_MPI
    if( mpi_mode == MPI_MODE_PIXEL && mpiSize > 0 ) {
        row_start = (width * j + mpiRank) % mpiSize;
        row_step = mpiSize;
    }
    #endif /* WITH_MPI */
    for(i=row_start; i<width; i+=row_step) {
        render_pixel(scn,width,x_scale,height,y_scale,i,j,mode,samples,&clr, &depth, max_optic_depth);
        dbl_image_set_pixel(img,i,j,&clr);
        if( depth_map != NULL ) {
            depth_clr.r = depth_clr.g = depth_clr.b = depth;
            dbl_image_set_pixel(depth_map,i,j,&depth_clr);
        }
    }

    return 0;
}

int resample_line(scene *scn, int width, double x_scale, int height, double y_scale, int j, stereo_mode mode, int samples, int aa_diff, int aa_depth, image_t *img, image_t *actual_img, image_t *prev_raw, image_t *prev_aa, int max_optic_depth)
{
    dbl_pixel_t clr;
    int ret = 0;
    int i=0;
    for(i=0; i<width; ++i) {
        ret += resample_pixel(scn, width, x_scale, height, y_scale, i, j, mode, samples, aa_diff, aa_depth, img, prev_raw, prev_aa, &clr, max_optic_depth);
        dbl_image_set_pixel(actual_img,i,j,&clr);
    }

    return ret;
}

struct thr_info {
    image_t *img;
    image_t *actual_img;
    image_t *prev_raw;
    image_t *prev_aa;
    image_t *depth_map;
    scene *scn;
    char *name;
    int width;
    int height;
    double x_scale;
    double y_scale;
    int samples;
    int aa_diff;
    int aa_depth;
    stereo_mode mode;
    int threads;
    int thr_offset;
    int pixel_count;
    int max_optic_depth;
};

void *render_lines_thread(void *arg)
{
    struct thr_info info;
    memcpy(&info,arg,sizeof(info));
    
    int j=0;
    struct timeval timer;
    if( info.thr_offset==0 )
        timer_start(&timer);
    int row_start = info.thr_offset;
    int row_step = info.threads;
    #ifdef WITH_MPI
    if( mpi_mode == MPI_MODE_ROW ) {
        row_start += mpiRank*info.threads;
        row_step *= mpiSize;
    }
    #endif /* WITH_MPI */
    for(j=row_start; j<info.height; j+=row_step) {
        render_line(info.scn, info.width, info.x_scale,
                    info.height, info.y_scale, j, info.mode, info.samples,
                    info.img, info.depth_map, info.max_optic_depth);

        if( info.thr_offset==0 && (j%10) == 0 ) {
            int num = image_active_saves();
            double remaining = timer_remaining(&timer, j, info.height+1);
            #ifdef WITH_MPI
            if( mpiRank == 0 || (mpiRank == 1 && mpi_mode == MPI_MODE_FRAME) ) {
            #endif /* WITH_MPI */
                if( num <= 0 )
                    printf("\r% 6.2f%% (%.2fs remaining)  ", 100.0*j/(info.height+1),remaining);
                else if( num == 1 )
                    printf("\r% 6.2f%%  (%i active save)   ", 100.0*j/info.height, num);
                else
                    printf("\r% 6.2f%%  (%i active saves)  ", 100.0*j/info.height, num);

                fflush(stdout);
            #ifdef WITH_MPI
            }
            #endif /* WITH_MPI */
        }
    }
    memcpy(arg,&info,sizeof(info));

    return 0;
}

void *resample_lines_thread(void *arg)
{
    struct thr_info info;
    memcpy(&info,arg,sizeof(info));
    
    int j=0;
    struct timeval timer;
    if( info.thr_offset==0 )
        timer_start(&timer);
    for(j=info.thr_offset; j<info.height; j+=info.threads) {
        info.pixel_count += resample_line(info.scn, info.width, info.x_scale,
                      info.height, info.y_scale, j, info.mode, info.samples,
                      info.aa_diff, info.aa_depth, info.img, info.actual_img,
                      info.prev_raw, info.prev_aa, info.max_optic_depth);

        if( info.thr_offset==0 && (j%10) == 0 ) {
            int num = image_active_saves();
            double remaining = timer_remaining(&timer, j, info.height+1);
            if( num <= 0 )
                printf("\r% 6.2f%% (%.2fs remaining)  ", 100.0*j/(info.height+1),remaining);
            else if( num == 1 )
                printf("\r% 6.2f%%  (%i active save)   ", 100.0*j/info.height, num);
            else
                printf("\r% 6.2f%%  (%i active saves)  ", 100.0*j/info.height, num);

            fflush(stdout);
        }
    }
    memcpy(arg,&info,sizeof(info));

    return 0;
}

int render_image(scene *scn, char *name, char *depth_name, int width, int height, int samples, stereo_mode mode, int threads, int aa_diff, int aa_depth, int aa_cache_frame, int max_optic_depth, image_t *img_copy, image_t *depth_copy)
{
    image_t *img = NULL;
    image_t *actual_img = NULL;
    image_t *depth_map = NULL;
    static image_t *prev_raw = NULL;
    static image_t *prev_aa = NULL;
    double x_scale = 1.0;
    double y_scale = 1.0;
    struct timeval timer;
    double seconds;
    int aa_pad = 0;

    printf("using %i thread%s to render image\n", threads, threads!=1?"s":"");

    if( mode == SIDE_SIDE_3D )
        x_scale = 0.5;
    if( mode == OVER_UNDER_3D )
        y_scale = 0.5;

    /* create output image */
    img = calloc(1,sizeof(image_t));
    dbl_image_init(img);
    /* make image one larger in each dimension (Whitted’s method) */
    if( recursive_aa )
        aa_pad = 1;
    image_set_size(img,width+aa_pad,height+aa_pad);
    if( mode != HIDEF_3D ) {
        vectNd_scale(&scn->cam.dirX,width/(double)height,&scn->cam.dirX);
    } else {
        vectNd_scale(&scn->cam.dirX,width/(double)1080,&scn->cam.dirX);
    }

    /* create depth-map image, if requested */
    if( depth_name != NULL ) {
        depth_map = calloc(1,sizeof(image_t));
        dbl_image_init(depth_map);
        image_set_size(depth_map,width,height);
    }

    actual_img = calloc(1,sizeof(image_t));
    image_init(actual_img);
    image_set_size(actual_img,width,height);

    struct thr_info *info;
    pthread_t *thr;
    info = calloc(threads,sizeof(struct thr_info));
    thr = calloc(threads,sizeof(pthread_t));

    timer_start(&timer);
    int i=0;
    for(i=0; i<threads; ++i) {
        info[i].img = img;
        info[i].scn = scn;
        info[i].name = name;
        info[i].width = width+aa_pad;
        info[i].height = height+aa_pad;
        info[i].x_scale = x_scale;
        info[i].y_scale = y_scale;
        info[i].samples = samples;
        info[i].mode = mode;
        info[i].threads = threads;
        info[i].thr_offset = i;
        info[i].actual_img = actual_img;
        info[i].depth_map = depth_map;
        info[i].max_optic_depth = max_optic_depth;

        if( threads > 1 )
            pthread_create(&thr[i],NULL,render_lines_thread,&info[i]);
        else
            render_lines_thread(&info[i]);
    }

    if( threads > 1 ) {
        for(i=0; i<threads; ++i) {
            pthread_join(thr[i],NULL);
        }
    }
    printf("\r                          \r");
    timer_elapsed(&timer,&seconds);
    printf("rendering took %.3fs\n", seconds);

    /* write initial image */
    if( name != NULL ) {
        #ifdef WITH_MPI
        if( !img_copy ) {
            mpi_collect_image(img);
            if( depth_name )
                mpi_collect_image(depth_map);
        }

        if( mpiRank == 0 ) {
        #endif /* WITH_MPI */

            if( image_active_saves() == 0 ) {
                timer_start(&timer);
                printf("\tsaving %s", name);
                if( threads > 1 )
                    image_save_bg(img,name,IMAGE_FORMAT);
                else
                    image_save(img,name,IMAGE_FORMAT);
                timer_elapsed(&timer,&seconds);
                printf(" (took %.3fs)\n", seconds);
            }

            if( depth_name != NULL ) {
                image_t norm;
                dbl_image_init(&norm);
                dbl_image_normalize(&norm,depth_map);
                image_save(&norm,depth_name,IMAGE_FORMAT);
                image_free(&norm);
            }

            if( recursive_aa && aa_cache_frame ) {
                timer_start(&timer);
                if( prev_raw!=NULL )
                    image_save(prev_raw,"prev_raw.png",IMAGE_FORMAT);
                if( prev_aa!=NULL )
                    image_save(prev_aa,"prev_aa.png",IMAGE_FORMAT);
                timer_elapsed(&timer,&seconds);
                printf("saving anti-aliasing cache images (took %.3fs)\n", seconds);
            }

        #ifdef WITH_MPI
        }
        #endif /* WITH_MPI */
    }

    if( name && img_copy && img ) {
        /* make a copy that will be visible by the calling function */
        image_copy(img_copy, img);
    }
    if( depth_name && depth_copy && depth_map ) {
        /* make a copy that will be visible by the calling function */
        image_copy(depth_copy, depth_map);
    }

    #if 0
    char fname[PATH_MAX];
    snprintf(fname, sizeof(fname), "mpi_rank_%i.%s", mpiRank, "png");
    image_save(img, fname, IMAGE_FORMAT);
    #endif /* 1 */

    if( recursive_aa ) {
        if( aa_depth >= 0 && aa_diff < 256 ) {
            printf("resampling image\n");
            timer_start(&timer);
            for(i=0; i<threads; ++i) {
                info[i].width = width;
                info[i].height = height;
                info[i].aa_diff = aa_diff;
                info[i].aa_depth = aa_depth;
                info[i].prev_raw = prev_raw;
                info[i].prev_aa = prev_aa;
                info[i].pixel_count = 0;
                pthread_create(&thr[i],NULL,resample_lines_thread,&info[i]);
            }

            int pixel_count=0;
            for(i=0; i<threads; ++i) {
                pthread_join(thr[i],NULL);
                pixel_count += info[i].pixel_count;
            }
            printf("\r              \r");
            timer_elapsed(&timer,&seconds);
            printf("\r\t%i pixels resampled. (%.2f%%)\n", pixel_count,
                    100.0*pixel_count/(width*height));
            printf("\tresampling took %.3fs\n", seconds);
        } else {
            /* simply copy img to actual_img */
            printf("\tcopying image without anti-aliasing\n");
            timer_start(&timer);
            dbl_pixel_t p;
            int j=0;
            for(j=0; j<height; ++j) {
                int i=0;
                for(i=0; i<width; ++i) {
                    dbl_image_get_pixel(img,i,j,&p);
                    dbl_image_set_pixel(actual_img,i,j,&p);
                }
            }
            timer_elapsed(&timer,&seconds);
            printf("\tcopy took %.3fs\n", seconds);
        }

        free(info); info=NULL;
        free(thr); thr=NULL;

        /* write image */
        if( name != NULL ) {
            timer_start(&timer);
            printf("\tsaving %s", name);
            if( threads > 1 )
                image_save_bg(actual_img,name,IMAGE_FORMAT);
            else
                image_save(actual_img,name,IMAGE_FORMAT);
            timer_elapsed(&timer,&seconds);
            printf(" (took %.3fs)\n", seconds);
        }

        /* update inter-frame caching of AA images */
        if( aa_cache_frame ) {
            if( prev_raw!=NULL ) {
                image_free(prev_raw);
                free(prev_raw); prev_raw=NULL;
            }
            prev_raw = img;
            img = NULL;

            if( prev_aa!=NULL ) {
                image_free(prev_aa);
                free(prev_aa); prev_aa=NULL;
            }
            prev_aa = actual_img;
            actual_img = NULL;
        } else {
            image_free(actual_img);
            free(actual_img); actual_img=NULL;
            image_free(img);
            free(img); img=NULL;
        }
    }
    if( actual_img ) {
        image_free(actual_img);
        free(actual_img); actual_img=NULL;
    }
    if( img ) {
        image_free(img);
        free(img); img=NULL;
    }
    if( info ) {
        free(info); info=NULL;
    }
    if( thr ) {
        free(thr); thr=NULL;
    }

    return 1;
}

#ifdef WITH_MPI
static int mpi_broadcast_scene(scene *scn) {
    int source_rank = 0;

    size_t length = -1;
    unsigned char *scene_buffer = NULL;

    if( mpiRank == source_rank ) {
        /* serialize scene on source */
        scene_write_yaml_buffer(scn, &scene_buffer, &length);
    }

    /* send size */
    int safe_len = (int)length;
    MPI_Bcast(&safe_len, 1, MPI_INT, source_rank, MPI_COMM_WORLD);
    length = (size_t)safe_len;

    if( mpiRank != source_rank ) {
        scene_buffer = calloc(length, sizeof(char));
    }

    /* send name */
    MPI_Bcast(scn->name, sizeof(scn->name), MPI_CHAR, source_rank, MPI_COMM_WORLD);

    /* send dimensions */
    MPI_Bcast(&scn->dimensions, 1, MPI_INT, source_rank, MPI_COMM_WORLD);

    /* send scene */
    MPI_Bcast(scene_buffer, length, MPI_CHAR, source_rank, MPI_COMM_WORLD);

    if( mpiRank != source_rank ) {
        /* parse scene_buffer on destination */
        scene_init(scn, scn->name, scn->dimensions);
        scene_read_yaml_buffer(scn, scene_buffer, length, 0);
    }

    free(scene_buffer); scene_buffer = NULL;

    return 0;
}

static int mpi_send_scene(scene *scn, int source_rank, int dest_rank) {
    size_t length = -1;
    unsigned char *scene_buffer = NULL;
    MPI_Status status;

    if( mpiRank == source_rank ) {
        /* serialize scene on source */
        scene_write_yaml_buffer(scn, &scene_buffer, &length);
    }

    /* send size */
    int safe_len = (int)length;
    if( mpiRank == source_rank ) {
        MPI_Send(&safe_len, 1, MPI_INT, dest_rank, 0, MPI_COMM_WORLD);
    } else {
        MPI_Recv(&safe_len, 1, MPI_INT, source_rank, 0, MPI_COMM_WORLD, &status);
    }
    length = (size_t)safe_len;

    if( mpiRank != source_rank ) {
        scene_buffer = calloc(length, sizeof(char));
    }

    /* send name */
    if( mpiRank == source_rank ) {
        MPI_Send(scn->name, sizeof(scn->name), MPI_CHAR, dest_rank, 0, MPI_COMM_WORLD);
    } else {
        MPI_Recv(scn->name, sizeof(scn->name), MPI_CHAR, source_rank, 0, MPI_COMM_WORLD, &status);
    }

    /* send dimensions */
    if( mpiRank == source_rank ) {
        MPI_Send(&scn->dimensions, 1, MPI_INT, dest_rank, 0, MPI_COMM_WORLD);
    } else {
        MPI_Recv(&scn->dimensions, 1, MPI_INT, source_rank, 0, MPI_COMM_WORLD, &status);
    }

    /* send scene */
    if( mpiRank == source_rank ) {
        MPI_Send(scene_buffer, length, MPI_CHAR, dest_rank, 0, MPI_COMM_WORLD);
    } else {
        MPI_Recv(scene_buffer, length, MPI_CHAR, source_rank, 0, MPI_COMM_WORLD, &status);
    }

    if( mpiRank != source_rank ) {
        /* parse scene_buffer on destination */
        scene_init(scn, scn->name, scn->dimensions);
        scene_read_yaml_buffer(scn, scene_buffer, length, 0);
    }

    free(scene_buffer); scene_buffer = NULL;

    return 0;
}

int mpi_send_image(image_t *img, int dest_rank) {
    /* send meta-data */
    MPI_Send(img, sizeof(image_t), MPI_BYTE, dest_rank, 0, MPI_COMM_WORLD);

    /* send pixel data */
    int pixels_size = img->width * img->height * img->pixel_width;
    MPI_Send(img->pixels, pixels_size, MPI_BYTE, dest_rank, 0, MPI_COMM_WORLD);

    return 0;
}

int mpi_recv_image(image_t *img, int source_rank) {
    MPI_Status status;

    /* recv meta-data */
    MPI_Recv(img, sizeof(image_t), MPI_BYTE, source_rank, 0, MPI_COMM_WORLD, &status);

    /* resize image, as needed */
    img->allocated = 0;
    img->pixels = NULL;
    image_set_size(img, img->width, img->height);

    /* recv pixel data */
    int pixels_size = img->width * img->height * img->pixel_width;
    MPI_Recv(img->pixels, pixels_size, MPI_BYTE, source_rank, 0, MPI_COMM_WORLD, &status);

    return 0;
}

int mpi_collect_image(image_t *img) {
    /* use heap rules to induce a tree rooted at rank 0 */
    int left = mpiRank * 2 + 1;
    int right = mpiRank * 2 + 2;
    int parent = (mpiRank-1) / 2;

    #if 0
    char fname[PATH_MAX];
    snprintf(fname, sizeof(fname), "img_%i.png", mpiRank);
    image_save(img, fname, IMG_TYPE_PNG);
    #endif /* 0 */

    /* read from children */
    if( right < mpiSize ) {
        image_t right_img;
        mpi_recv_image(&right_img, right);
        image_add(img, &right_img, img);
        image_free(&right_img);
    }
    if( left < mpiSize ) {
        image_t left_img;
        mpi_recv_image(&left_img, left);
        image_add(img, &left_img, img);
        image_free(&left_img);
    }

    /* send combined results to parent */
    if( parent != mpiRank ) {
        mpi_send_image(img, parent);
    }

    return 0;
}
#endif /* WITH_MPI */

int print_help_info(int argc, char **argv)
{
    #ifdef WITH_MPI
    /* only rank 0 should print this */
    if( mpiRank != 0 )
        return 0;
    #endif /* WITH_MPI */

    printf("Usage\n"
           "\t%s [options]\n"
           "\n"
           "\t-a diff,depth\tAnti-aliasing options\n"
           #ifdef WITH_MPI
           "\t-b mode\t\tmpi render granularity mode (p,t,f,F)\n"
           "\t\t\t\tp: pixel level parallelism\n"
           "\t\t\t\tr: row level parallelism\n"
           "\t\t\t\tf: frame level parallelism\n"
           "\t\t\t\tF: frame level with rendering by rank 0\n"
           #endif /* WITH_MPI */
           "\t-c\t\tEnable anti-aliasing cache\n"
           "\t-d dimension\tNumber of spacial dimension to use\n"
           "\t-e num\t\tLast frame number to render\n"
           "\t-f num\t\tNumber of frames to render\n"
           "\t-g\t\tEnable writing of depthmap image(s)\n"
           "\t-h height\tHeight of output image (in pixels)\n"
           "\t-i num\t\tInitial frame to render\n"
           "\t-k num\t\tNumber of clusters per level when grouping objects\n"
           "\t-l num\t\tMaximum recusion depth for reflection/refraction\n"
           "\t-m mode\t\tStereoscopic rendering mode (s,o,a,h,m)\n"
           "\t\t\t\ts: side by side (sbs2l)\n"
           "\t\t\t\to: over/under (ab2l)\n"
           "\t\t\t\ta: red/blue anaglyph (arbg)\n"
           "\t\t\t\th: high-def 1080p 3D (high)\n"
           "\t\t\t\tm: monoscopic [default]\n"
           "\t-n samples\tResampling count for each pixel\n"
           "\t-p\t\tDisable specular high-lighting\n"
           "\t-q quality\tPreset quality levels (high,med,low,fast)\n"
           "\t-r mode,vFov,[hFov]\tRadial camera, mode={spherical,cylindrical}\n"
           "\t-s scene.so\tShared object that specifies the scene\n"
           "\t-t threads\tThreads to use\n"
           "\t-u scene_config\tScene specific options string\n"
           "\t-w width\tWidth of output image (in pixels)\n"
           #ifdef WITH_YAML
           "\t-y\t\tWrite YAML file(s)\n"
           #endif /* WITH_YAML */
           "\t-?\t\tPrint this help message\n",
           argv[0]);
    fflush(stdout);

    return 0;
}

int main(int argc, char **argv)
{
    int dimensions = 3;
    int width = 1920;
    int height = 1080;
    int frames = 300;
    int frames_given = 0;
    int initial_frame = 0;
    int last_frame = -1;
    scene scn;
    char fname[PATH_MAX];
    char dname[PATH_MAX];
    char dname2[PATH_MAX];
    char depth_dname[PATH_MAX];
    char res_str[24];
    stereo_mode stereo = MONO;
    char *mode_str = "";
    char *cam_str = "";
    int samples = 1;
    int threads = 1;
    int cluster_k = 6;
    int aa_depth = 4;
    int aa_diff = 20;
    char *aa_str = NULL;
    char *depth_str=NULL, *diff_str=NULL;
    int max_optic_depth = 128;
    int aa_cache_frame = 0;
    void *dl_handle = NULL;
    int (*custom_scene)(scene *scn, int dimensions, int frame, int frames, char *config) = NULL;
    int (*custom_frame_count)(int dimensions, char *config) = NULL;
    int (*custom_scene_cleanup)() = NULL;
    char *scene_config = NULL;
    int record_depth_map = 0;
    int enable_vr = 0;
    int enable_pano = 0;
    char *radial_mode = NULL;
    char *vFov_str = NULL;
    char *hFov_str = NULL;
    double camera_v_fov = M_PI;
    double camera_h_fov = 2.0 * M_PI;
    char obj_dir[PATH_MAX] = "objects";
    #ifdef WITH_YAML
    int write_yaml = 0;
    #endif /* WITH_YAML */

    #ifdef WITH_MPI
    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpiSize);

    if( mpiSize > 1 )
        mpi_mode = MPI_MODE_ROW;
    #endif /* WITH_MPI */

    /* process command-line options */
    int ch = '\0';
    /* unused: b,j,x,z */
    while( (ch=getopt(argc, argv, ":a:b:cd:e:f:gh:i:k:l:m:n:o:pq:r:s:t:u:vw:y3:?"))!=-1 ) {
        switch(ch) {
            case 'a':
                aa_str = strdup(optarg);
                diff_str = strtok(aa_str,",");
                depth_str = strtok(NULL,",");
                if( diff_str!=NULL )
                    aa_diff = atoi(diff_str);
                if( depth_str!=NULL )
                    aa_depth = atoi(depth_str);
                free(aa_str); aa_str=NULL;
                printf("anti-aliasing = diff=%i,depth=%i\n", aa_diff, aa_depth);
                break;
            case 'b':
                #ifdef WITH_MPI
                switch( optarg[0] ) {
                    case 'p':
                        /* pixel cyclic */
                        mpi_mode = MPI_MODE_PIXEL;
                        break;
                    case 'r':
                        /* row cyclic */
                        mpi_mode = MPI_MODE_ROW;
                        break;
                    case 'f':
                        /* frame cyclic */
                        mpi_mode = MPI_MODE_FRAME;
                        break;
                    case 'F':
                        /* frame cyclic */
                        mpi_mode = MPI_MODE_FRAME2;
                        break;
                    default:
                        fprintf(stderr, "Unknown MPI blocking mode '%s' (Valid values: p,b,f,F).\n", optarg);
                        #ifdef WITH_MPI
                        MPI_Finalize();
                        #endif /* WITH_MPI */
                        exit(1);
                        break;
                }
                #else
                fprintf(stderr,"Not compiled with MPI support, remove the -b option.\n");
                exit(1);
                #endif /* WITH_MPI */
                break;
            case 'c':
                aa_cache_frame = 1;
                printf("inter-frame anti-aliasing cache enabled\n");
                break;
            case 'd':
                dimensions = atoi(optarg);
                if( dimensions < 3 ) {
                    fprintf(stderr, "Number of dimensions %i(flag 'd') is invalid, must be 3 or greated.\n", dimensions);
                    #ifdef WITH_MPI
                    MPI_Finalize();
                    #endif /* WITH_MPI */
                    exit(1);
                }
                printf("rendering in %id\n", dimensions);
                break;
            case 'e':
                last_frame = atoi(optarg);
                printf("end frame = %i\n", last_frame);
                break;
            case 'f':
                frames_given = 1;
                frames = atoi(optarg);
                printf("%i frames\n", frames);
                break;
            case 'g':
                record_depth_map = 1;
                printf("record_depth_map = yes\n");
                break;
            case 'h':
                height = atoi(optarg);
                printf("height = %i\n", height);
                break;
            case 'i':
                initial_frame = atoi(optarg);
                printf("initial frame = %i\n", initial_frame);
                break;
            case 'k':
                cluster_k = atoi(optarg);
                printf("clusters per level = %i\n", cluster_k);
                break;
            case 'l':
                max_optic_depth = atoi(optarg);
                printf("reflection/refraction depth limit = %i\n", max_optic_depth);
                break;
            case 'm':
            case '3':
                /* for mode_str explanations see:
                 * https://trac.ffmpeg.org/wiki/Stereoscopic
                 */
                switch(optarg[0]) {
                    case 'S':
                    case 's':
                        stereo = SIDE_SIDE_3D;
                        mode_str = "sbs2l";
                        printf("stereo = SIDE_SIDE_3D\n");
                        break;
                    case 'O':
                    case 'o':
                        stereo = OVER_UNDER_3D;
                        mode_str = "ab2l";
                        printf("stereo = OVER_UNDER_3D\n");
                        break;
                    case 'A':
                    case 'a':
                        stereo = ANAGLYPH_3D;
                        mode_str = "arbg";
                        printf("stereo = ANAGLYPH_3D\n");
                        break;
                    case 'H':
                    case 'h':
                        stereo = HIDEF_3D;
                        width = 1920;
                        height = 2205;
                        mode_str = "high";
                        printf("stereo = HIDEF_3D\n");
                        break;
                    case 'M':
                    case 'm':
                    default:
                        stereo = MONO;
                        mode_str = "";
                        printf("stereo = MONO\n");
                        break;
                }
                break;
            case 'n':
                samples = atoi(optarg);
                printf("samples = %i\n", samples);
                break;
            case 'o':
                strncpy(obj_dir,optarg,sizeof(obj_dir));
                break;
            case 'p':
                #ifdef WITH_SPECULAR
                specular_enabled = 0;
                #else
                printf("Specular highlights disabled, -p ignored.\n");
                #endif /* WITH_SPECULAR */
                break;
            case 'q':
                switch(optarg[0]) {
                    case 'h':
                    case 'H':
                        /* settings for high quality */
                        aa_depth = 17;
                        aa_diff = 1;
                        aa_cache_frame = 0;
                        max_optic_depth = 128;
                        break;
                    case 'm':
                    case 'M':
                    default:
                        /* settings for medium quality */
                        aa_depth = 2;
                        aa_diff = 1;
                        aa_cache_frame = 1;
                        max_optic_depth = 20;
                        break;
                    case 'l':
                    case 'L':
                        /* settings for low rendering */
                        aa_depth = 0;
                        aa_diff = 255;
                        aa_cache_frame = 0;
                        max_optic_depth = 5;
                        break;
                        break;
                    case 'f':
                    case 'F':
                        /* settings for fastest rendering */
                        aa_depth = 0;
                        aa_diff = 255;
                        aa_cache_frame = 0;
                        max_optic_depth = 1;
                        break;
                }
                printf("anti-aliasing = diff=%i,depth=%i\n", aa_diff, aa_depth);
                printf("aa_cache_frame = %i\n", aa_cache_frame);
                printf("reflection/refraction depth limit = %i\n", max_optic_depth);
                break;
            case 'r':
                /* enable radial camera mode */
                enable_vr = 0;
                enable_pano = 0;
                cam_str = "unknown";
                radial_mode = strtok(optarg,",");
                vFov_str = strtok(NULL,",");
                hFov_str = strtok(NULL,",");
                if( radial_mode == NULL )
                    radial_mode = optarg;
                if( radial_mode != NULL ) {
                    if( radial_mode[0] == 's' || radial_mode[0] == 'S' ) {
                        enable_vr = 1;
                        cam_str = "vr";
                        printf("VR = enabled\n");
                    } else if( radial_mode[0] == 'c' || radial_mode[0] == 'C' ) {
                        enable_pano = 1;
                        cam_str = "pano";
                        printf("PANO = enabled\n");
                    } else {
                        fprintf(stderr,"Unrecognized radial mode: %s\n", radial_mode);
                        #ifdef WITH_MPI
                        MPI_Finalize();
                        #endif /* WITH_MPI */
                        exit(1);
                    }
                }
                if( vFov_str != NULL ) {
                    camera_v_fov = atof(vFov_str) * M_PI / 180.0;
                }
                if( hFov_str != NULL ) {
                    camera_h_fov = atof(hFov_str) * M_PI / 180.0;
                }
                printf("    vFov = %g\n", camera_v_fov * 180.0 / M_PI);
                printf("    hFov = %g\n", camera_h_fov * 180.0 / M_PI);
                break;
            case 's':
                printf("Loading scene object %s\n", optarg);
                dl_handle = dlopen(optarg,RTLD_NOW);
                if( !dl_handle ) {
                    fprintf(stderr, "%s\n", dlerror());
                    #ifdef WITH_MPI
                    MPI_Finalize();
                    #endif /* WITH_MPI */
                    exit(1);
                } else {
                    *(void **) (&custom_scene) = dlsym(dl_handle, "scene_setup");
                    *(void **) (&custom_frame_count) = dlsym(dl_handle, "scene_frames");
                    *(void **) (&custom_scene_cleanup) = dlsym(dl_handle, "scene_cleanup");
                }
                break;
            case 't':
                threads = atoi(optarg);
                printf("threads = %i\n", threads);
                break;
            case 'u':
                scene_config = strdup(optarg);
                printf("scene config string = %s\n", scene_config);
                break;
            case 'w':
                width = atoi(optarg);
                printf("width = %i\n", width);
                break;
            case 'y':
                #ifdef WITH_YAML
                write_yaml = 1;
                #else
                fprintf(stderr, "%s not compiled with YAML support.  Needs to be compiled with -DWITH_YAML.", argv[0]);
                #ifdef WITH_MPI
                MPI_Finalize();
                #endif /* WITH_MPI */
                exit(1);
                #endif /* WITH_YAML */
                break;
            case '?':
            case ':':
                print_help_info(argc,argv);
                #ifdef WITH_MPI
                MPI_Finalize();
                #endif /* WITH_MPI */
                exit(1);
                break;
            default:
                printf("Unknown option '%c' (%d)\n", ch, ch);
                printf("Try the -? option\n");
                #ifdef WITH_MPI
                MPI_Finalize();
                #endif /* WITH_MPI */
                exit(1);
                break;
        }
    }

    if( custom_frame_count != NULL && !frames_given ) {
        frames = (*custom_frame_count)(dimensions,scene_config);
        printf("Scene requested %i frames. (override by adding a -f flag after the -s flag).\n", frames);
    }

    if( last_frame < 0 )
        last_frame = frames-1;

    /* load objects */
    register_objects(obj_dir);

    #ifdef WITH_MPI
    int frames_running = 0;
    #endif /* WITH_MPI */

    struct timeval global_timer;
    double seconds;
    timer_start(&global_timer);
    int i=0;
    for(i=0; i<frames && i<=last_frame; ++i) {

        #ifdef WITH_MPI
        int render_rank = 0;
        if( mpiSize > 1 )
            render_rank = (((i-initial_frame)%(mpiSize-1))+1);
        if( mpi_mode == MPI_MODE_FRAME2 ) {
            /* shift by one so rank zero can create scened before it starts
             * rendering. */
            render_rank = ((i-initial_frame)+1)%mpiSize;
        }
        /* skip certain frames when in frames mode */
        if( mpi_mode==MPI_MODE_FRAME && mpiRank!=0 && mpiRank!=render_rank ) {
            continue;
        }
        if( mpi_mode==MPI_MODE_FRAME2 && mpiRank!=0 && mpiRank!=render_rank ) {
            continue;
        }

        if( mpiRank == 0 ) {
            /* only rank 0 computes the scene */
        #endif /* WITH_MPI */
            /* set up scene */
            if( custom_scene!=NULL ) {
                (*custom_scene)(&scn,dimensions,i,frames,scene_config);
            } else {
                scene_setup(&scn,dimensions,i,frames,scene_config);
            }

            #ifdef WITH_YAML
            if( write_yaml ) {
                char *output_dir = "yaml";
                char yaml_fname[PATH_MAX];
                mkdir(output_dir,0700);
                utimes(output_dir, NULL);
                snprintf(dname,sizeof(dname),"%s/%s_%id", output_dir, scn.name, dimensions);
                mkdir(dname,0700);
                utimes(dname, NULL);
                snprintf(yaml_fname, sizeof(yaml_fname), "%s/%s_%05i.yaml", dname, scn.name, i);
                scene_write_yaml(&scn, yaml_fname);
            }
            #endif /* WITH_YAML */

        #ifdef WITH_MPI
        }
        #endif /* WITH_MPI */

        /* This has to happen after the call to scene setup function, in case
         * there is persistent interframe data that needs to be updated. */
        if( i < initial_frame ) {
            printf("Skipping frame %i (less than inital frame %i) \n", i, initial_frame);
            #ifdef WITH_MPI
            if( mpiRank == 0 )
            #endif /* WITH_MPI */
                scene_free(&scn);
            continue;
        }

        #ifdef WITH_MPI
        if( mpi_mode == MPI_MODE_ROW || mpi_mode == MPI_MODE_PIXEL ) {
            /* broadcast scene */
            mpi_broadcast_scene(&scn);
        } else if( mpi_mode == MPI_MODE_FRAME || mpi_mode == MPI_MODE_FRAME2 ) {
            /* send scene to appropriate rank */
            if( render_rank != 0 ) {
                mpi_send_scene(&scn, 0, render_rank);
            }
            ++frames_running;
        }
        #endif /* WITH_MPI */

        /* construct output image filename */
        char *output_dir = "images";
        mkdir(output_dir,0700);
        utimes(output_dir, NULL);
        snprintf(dname,sizeof(dname),"%s/%s", output_dir, scn.name);
        mkdir(dname,0700);
        utimes(dname,NULL);
        snprintf(dname,sizeof(dname),"%s/%s/%id%s%s%s%s", output_dir, scn.name, dimensions, mode_str[0]=='\0'?"":"_", mode_str, cam_str[0]=='\0'?"":"_", cam_str);
        mkdir(dname,0700);
        utimes(dname,NULL);
        snprintf(res_str,sizeof(res_str),"%ix%i", width, height);
        snprintf(dname2,sizeof(dname2),"%s/%s", dname, res_str);
        mkdir(dname2,0700);
        utimes(dname2, NULL);
        if( record_depth_map ) {
            snprintf(depth_dname,sizeof(depth_dname),"%s/depth", dname2);
            mkdir(depth_dname,0700);
            utimes(depth_dname, NULL);
        }
        char *ext = "unknown";
        #ifdef WITH_PNG
        if( IMAGE_FORMAT == IMG_TYPE_PNG )
            ext = "png";
        #endif /* WITH_PNG */
        #ifdef WITH_JPEG
        if( IMAGE_FORMAT == IMG_TYPE_JPEG )
           ext = "jpg";
        #endif /* WITH_JPEG */
        snprintf(fname,sizeof(fname),"%s/%s_%s_%04i.%s", dname2, scn.name, res_str, i, ext);
        char *depth_fname = NULL;
        if( record_depth_map ) {
            depth_fname = calloc(PATH_MAX,sizeof(char));
            snprintf(depth_fname,PATH_MAX,"%s/%s_%s_%04i.%s", depth_dname, scn.name, res_str, i, ext);
        }

        image_t *img = NULL;
        image_t *depth_img = NULL;
        #ifdef WITH_MPI
        if( mpi_mode == MPI_MODE_FRAME || mpi_mode == MPI_MODE_FRAME2 ) {
            img = calloc(1, sizeof(image_t));
            image_init(img);
            if( depth_fname ) {
                depth_img = calloc(1, sizeof(image_t));
                image_init(depth_img);
            }
        }
        #endif /* WITH_MPI */

        /* start timer for render */
        struct timeval timer;
        timer_start(&timer);

        #ifdef WITH_MPI
        if( mpi_mode == MPI_MODE_ROW || mpi_mode == MPI_MODE_PIXEL || mpiRank == render_rank ) {
        #endif /* WITH_MPI */
            printf("Scene has %i objects and %i lights\n", scn.num_objects, scn.num_lights);
            scene_cluster(&scn, cluster_k);

            scene_validate_objects(&scn);

            /* setup camera, as requested */
            if( enable_vr ) {
                scn.cam.type = CAMERA_VR;
            } else if( enable_pano ) {
                scn.cam.type = CAMERA_PANO;
            }
            camera_aim(&scn.cam);

            /* do actual rendering */
            #ifdef WITH_MPI
            printf("rank %i: rendering frame %i/%i \n", mpiRank, i, frames);
            #else
            printf("rendering frame %i/%i \n", i, frames);
            #endif /* WITH_MPI */
            render_image(&scn, fname, depth_fname, width, height, samples, stereo, threads, aa_diff, aa_depth, aa_cache_frame, max_optic_depth, img, depth_img);

        #ifdef WITH_MPI
            if( mpiRank!=0 && (mpi_mode == MPI_MODE_FRAME || mpi_mode == MPI_MODE_FRAME2) ) {
                if( img ) {
                    MPI_Send(fname, sizeof(fname), MPI_CHAR, 0, 0, MPI_COMM_WORLD);
                    mpi_send_image(img, 0);
                }
                if( depth_img && depth_fname ) {
                    MPI_Send(depth_fname, sizeof(depth_fname), MPI_CHAR, 0, 0, MPI_COMM_WORLD);
                    mpi_send_image(depth_img, 0);
                }
            }
        }

        if( mpiRank==0
            && ((mpi_mode == MPI_MODE_FRAME && frames_running >= (mpiSize-1))
                || (mpi_mode == MPI_MODE_FRAME2 && frames_running >= mpiSize)
                || i == last_frame) ) {
            /* Use the non-background saves here, to reduce memory usage. */

            if( mpi_mode == MPI_MODE_FRAME2 ) {
                /* save locally rendered frame */
                if( img )
                    image_save(img, fname, IMAGE_FORMAT);
                if( depth_img )
                    image_save(depth_img, depth_fname, IMAGE_FORMAT);
            }

            /* fetch and save remotely rendered frames */
            for(int rank=1; rank<=frames_running && rank < mpiSize; ++rank) {
                MPI_Status status;

                if( img ) {
                    MPI_Recv(fname, sizeof(fname), MPI_CHAR, rank, 0, MPI_COMM_WORLD, &status);
                    mpi_recv_image(img, rank);
                    image_save(img, fname, IMAGE_FORMAT);
                    image_free(img);
                    image_init(img);    /* prepare for next iteration */
                }
                if( depth_img && depth_fname ) {
                    MPI_Recv(depth_fname, sizeof(depth_fname), MPI_CHAR, rank, 0, MPI_COMM_WORLD, &status);
                    mpi_recv_image(depth_img, rank);
                    image_save(depth_img, depth_fname, IMAGE_FORMAT);
                    image_free(depth_img);
                    image_init(depth_img);  /* prepare for next iteration */
                }
            }
            frames_running = 0;
        }

        /* free images for ranks other than 0 */
        if( img ) {
            image_free(img);
            free(img); img = NULL;
        }
        if( depth_img ) {
            image_free(depth_img);
            free(depth_img); depth_img = NULL;
        }
        #endif /* WITH_MPI */

        /* cleanup */
        scene_free(&scn);
        if( depth_fname != NULL ) {
            free(depth_fname); depth_fname = NULL;
        }

        #ifdef WITH_MPI
        if( (mpi_mode != MPI_MODE_FRAME || mpiRank == 0)
            && frames_running == 0 ) {
        #endif /* WITH_MPI */
            /* record frame time */
            timer_elapsed(&timer,&seconds);
            printf("%s took %0.2fs to render\n", fname, seconds);
            timer_elapsed(&global_timer,&seconds);
            int total_frames = i-initial_frame+1;
            printf("\t%i frame%s took %0.2fs (avg. %0.3fs)\n",
                total_frames, (total_frames!=1)?"s":"",
                seconds, seconds/total_frames);
            int remaining_frames = frames - i - 1;
            printf("\t%0.2fs remaining.\n", (seconds/total_frames) * remaining_frames);
        #ifdef WITH_MPI
        }
        #endif /* WITH_MPI */
    }   /* frames */

    timer_elapsed(&global_timer,&seconds);
    printf("\n%i frame%s took %0.2fs (avg. %0.3fs)\n",
        (last_frame+1)-initial_frame,(((last_frame+1)-initial_frame)!=1)?"s":"",
        seconds,seconds/((last_frame+1)-initial_frame));

    int active_saves = 0;
    while( (active_saves = image_active_saves()) > 0 ) {
        printf("\rPausing to allow %i I/O thread%s to finish. ", active_saves, ((active_saves==1)?"":"s"));
        fflush(stdout);
        usleep(100);
    }
    printf("\r                                               \rdone.\n");

    /* cleanup after scene */
    if( custom_scene_cleanup != NULL
        #ifdef WITH_MPI
        && mpiRank == 0
        #endif /* WITH_MPI */
        ) {
        (*custom_scene_cleanup)();
    }

    /* free custom scene library */
    if( dl_handle 
        #ifdef WITH_VALGRIND
        && !RUNNING_ON_VALGRIND
        #endif /* WITH_VALGRIND */
        ) {
        dlclose(dl_handle); dl_handle = NULL;
    }

    /* free object type registry */
    unregister_objects();

    #ifdef WITH_VALGRIND
    if( RUNNING_ON_VALGRIND ) {
        /* without this, valgrind may report non-existant memory leaks */
        printf("Sleeping for a bit.\n");
        sleep(1);
    }
    #endif /* WITH_VALGRIND */

    #ifdef WITH_MPI
    printf("rank %i finalizing.\n", mpiRank);
    fflush(stdout);
    MPI_Finalize();
    #endif /* WITH_MPI */

    return 0;
}
