/*
 * camera.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "vectNd.h"
#include "object.h"
#include "camera.h"

const char *CAMERA_TYPE_STRING[] = {
    FOREACH_CAMERA_TYPE(GENERATE_STRING)
};

int camera_alloc(camera *cam, int dim)
{
    vectNd_calloc(&cam->viewPoint,dim);
    vectNd_calloc(&cam->viewTarget,dim);
    vectNd_calloc(&cam->up,dim);

    vectNd_calloc(&cam->pos,dim);
    vectNd_calloc(&cam->leftEye,dim);
    vectNd_calloc(&cam->rightEye,dim);
    vectNd_calloc(&cam->dirX,dim);
    vectNd_calloc(&cam->dirY,dim);
    vectNd_calloc(&cam->imgOrig,dim);

    vectNd_calloc(&cam->localX,dim);
    vectNd_calloc(&cam->localY,dim);
    vectNd_calloc(&cam->localZ,dim);

    camera_init(cam);

    cam->prepared = 0;

    return 1;
}

int camera_free(camera *cam)
{
    vectNd_free(&cam->viewPoint);
    vectNd_free(&cam->viewTarget);
    vectNd_free(&cam->up);

    vectNd_free(&cam->pos);
    vectNd_free(&cam->leftEye);
    vectNd_free(&cam->rightEye);
    vectNd_free(&cam->dirX);
    vectNd_free(&cam->dirY);
    vectNd_free(&cam->imgOrig);

    vectNd_free(&cam->localX);
    vectNd_free(&cam->localY);
    vectNd_free(&cam->localZ);

    return 1;
}

int camera_init(camera *cam)
{
    cam->type = CAMERA_NORMAL;

    vectNd_reset(&cam->viewPoint);
    vectNd_reset(&cam->viewTarget);
    vectNd_reset(&cam->up);
    cam->rotation = 0.0;
    cam->eye_offset = EYE_OFFSET;

    cam->zoom = 1.0;
    cam->flip_x = 0;
    cam->flip_y = 0;
    cam->flatten = 0;

    vectNd_reset(&cam->pos);
    vectNd_reset(&cam->leftEye);
    vectNd_set(&cam->leftEye,0,-EYE_OFFSET);
    vectNd_reset(&cam->rightEye);
    vectNd_set(&cam->rightEye,0,EYE_OFFSET);
    vectNd_reset(&cam->dirX);
    vectNd_set(&cam->dirX,0,1.0);
    vectNd_reset(&cam->dirY);
    vectNd_set(&cam->dirY,1,1.0);
    vectNd_reset(&cam->imgOrig);
    vectNd_set(&cam->imgOrig,2,2.0);

    vectNd_reset(&cam->localX);
    vectNd_reset(&cam->localY);
    vectNd_reset(&cam->localZ);
    vectNd_set(&cam->localX,0,1.0);
    vectNd_set(&cam->localY,1,1.0);
    vectNd_set(&cam->localZ,2,1.0);

    cam->hFov = 2.0 * M_PI;
    cam->vFov = M_PI / 2.0;

    cam->focal_distance = 100.0;
    cam->aperture_radius = 0.0;

    return 1;
}

int camera_reset(camera *cam)
{
    double focalLength=0, xLen=0, yLen=0;

    cam->prepared = 0;
    vectNd_dist(&cam->pos,&cam->imgOrig,&focalLength);
    vectNd_l2norm(&cam->dirX,&xLen);
    vectNd_l2norm(&cam->dirY,&yLen);

    camera_init(cam);
    vectNd_reset(&cam->dirX);
    vectNd_set(&cam->dirX,0,xLen);
    vectNd_reset(&cam->dirY);
    vectNd_set(&cam->dirY,1,yLen);
    vectNd_reset(&cam->imgOrig);
    vectNd_set(&cam->imgOrig,2,focalLength);

    cam->hFov = 2.0 * M_PI;
    cam->vFov = M_PI / 2.0;

    return 1;
}

int camera_aim(camera *cam)
{
    double up_len = 0.0;
    vectNd_l2norm(&cam->up, &up_len);
    vectNd *up = NULL;
    if( up_len > 0 ) {
        up = calloc(1,sizeof(vectNd));
        vectNd_calloc(up, cam->up.n);
        vectNd_copy(up, &cam->up);
    }

    if( up != NULL ) {
        double curr = 0;
        double delta = M_PI/10;
        double angle = 0;
        double last_angle = 0;
        camera tmp_cam;

        camera_alloc(&tmp_cam, cam->viewPoint.n);
        camera_set_aim(&tmp_cam, &cam->viewPoint, &cam->viewTarget, &cam->up, 0.0);
        camera_aim_naive(&tmp_cam);
        vectNd_angle(up,&tmp_cam.dirY,&angle);

        while( fabs(delta) > (EPSILON/1000) ) {
            last_angle = angle;

            /* copy aiming information back into temporary camera */
            camera_set_aim(&tmp_cam, &cam->viewPoint, &cam->viewTarget, &cam->up, curr);
            camera_aim_naive(&tmp_cam);
            vectNd_angle(up,&tmp_cam.dirY,&angle);

            if( angle >= last_angle )
                delta = -delta / 2.0;

            curr += delta;
        }
        cam->leveling = curr;

        /* clean up */
        camera_free(&tmp_cam);
    }

    return camera_aim_naive(cam);
}

int camera_aim_naive(camera *cam)
{
    int dim = cam->pos.n;
    vectNd posX;
    vectNd posY;

    /* reconstruct original parameters from camera */
    vectNd *pos = calloc(1,sizeof(vectNd));
    vectNd *target = calloc(1,sizeof(vectNd));
    vectNd_calloc(pos, cam->viewPoint.n);
    vectNd_calloc(target, cam->viewTarget.n);
    vectNd_copy(pos, &cam->viewPoint);
    vectNd_copy(target, &cam->viewTarget);
    double rot = cam->rotation + cam->leveling;
    double zoom = cam->zoom;
    int flip_x = cam->flip_x;
    int flip_y = cam->flip_y;
    int flatten = cam->flatten;
    double hFov = cam->hFov;
    double vFov = cam->vFov;
    double aperture_radius = cam->aperture_radius;
    double focal_distance = cam->focal_distance;

    /* reset camera to default location and orientation */
    camera_reset(cam);

    /* copy aim parameters back into camera */
    vectNd_copy(&cam->viewPoint, pos);
    vectNd_copy(&cam->viewTarget, target);
    cam->rotation = rot;
    cam->eye_offset = EYE_OFFSET;
    cam->zoom = zoom;
    cam->flip_x = flip_x;
    cam->flip_y = flip_y;
    cam->flatten = flatten;
    cam->hFov = hFov;
    cam->vFov = vFov;
    cam->aperture_radius = aperture_radius;
    cam->focal_distance = focal_distance;

    double targetDist = 0.0;
    double focalLen = 0.0;
    vectNd_dist(pos,target,&targetDist);
    vectNd_l2norm(&cam->imgOrig,&focalLen);
    vectNd_unitize(&cam->imgOrig);
    vectNd_scale(&cam->imgOrig,targetDist,&cam->imgOrig);
    vectNd_scale(&cam->dirX,targetDist/focalLen,&cam->dirX);
    vectNd_scale(&cam->dirY,targetDist/focalLen,&cam->dirY);

    /* compute points to track x and y vectors */
    vectNd_alloc(&posX,cam->dirX.n); 
    vectNd_add(&cam->imgOrig,&cam->dirX,&posX);
    vectNd_alloc(&posY,cam->dirY.n); 
    vectNd_add(&cam->imgOrig,&cam->dirY,&posY);

    /* move camera to pos */
    vectNd_add(&cam->pos, pos, &cam->pos);
    vectNd_add(&cam->leftEye, pos, &cam->leftEye);
    vectNd_add(&cam->rightEye, pos, &cam->rightEye);
    vectNd_add(&posX, pos, &posX);
    vectNd_add(&posY, pos, &posY);
    vectNd_add(&cam->imgOrig, pos, &cam->imgOrig);

    /* rotate the camera in the screen plane before aiming */
    vectNd_rotate(&posX,&cam->pos,0,1,rot,&posX);
    vectNd_rotate(&posY,&cam->pos,0,1,rot,&posY);
    vectNd_rotate(&cam->imgOrig,&cam->pos,0,1,rot,&cam->imgOrig);
    vectNd_rotate(&cam->leftEye,&cam->pos,0,1,rot,&cam->leftEye);
    vectNd_rotate(&cam->rightEye,&cam->pos,0,1,rot,&cam->rightEye);

    /* rotate camera to look in that direction */
    for(int i=0; i<dim; ++i) {
        for(int j=0; j<dim; ++j) {
            /* need different dimensions to form a plane of rotation */
            if( i==j )
                continue;

            /* determine angle of rotation in i,j plane */
            double cam_rise = -1;
            double cam_run = -1;
            double tar_rise = -1;
            double tar_run = -1;
            cam_rise = cam->imgOrig.v[j] - cam->pos.v[j];
            cam_run = cam->imgOrig.v[i] - cam->pos.v[i];
            tar_rise = target->v[j] - cam->pos.v[j];
            tar_run = target->v[i] - cam->pos.v[i];
            if( fabs(cam_rise) < EPSILON ) cam_rise = 0;
            if( fabs(cam_run) < EPSILON ) cam_run = 0;
            if( fabs(tar_rise) < EPSILON ) tar_rise = 0;
            if( fabs(tar_run) < EPSILON ) tar_run = 0;
            double cam_angle = atan2(cam_rise,cam_run);
            double tar_angle = atan2(tar_rise,tar_run);
            if( tar_angle < cam_angle )
                tar_angle += 2*M_PI;
            double angle = tar_angle - cam_angle;

            /* rotate defining points for camera */
            vectNd_rotate(&posX,&cam->pos,i,j,angle,&posX);
            vectNd_rotate(&posY,&cam->pos,i,j,angle,&posY);
            vectNd_rotate(&cam->imgOrig,&cam->pos,i,j,angle,&cam->imgOrig);
            vectNd_rotate(&cam->leftEye,&cam->pos,i,j,angle,&cam->leftEye);
            vectNd_rotate(&cam->rightEye,&cam->pos,i,j,angle,&cam->rightEye);
        }
    }

    /* recompute x and y direction vectors */
    vectNd_sub(&posX,&cam->imgOrig,&cam->dirX);
    vectNd_sub(&posY,&cam->imgOrig,&cam->dirY);

    if( 0 && cam->flatten ) {
        for(int i=3; i<cam->pos.n; ++i) {
            double avg = (cam->leftEye.v[i] + cam->rightEye.v[i])/2.0;
            cam->leftEye.v[i] = avg;
            cam->leftEye.v[i] = avg;
        }
    }

    /* setup 'local' vectors for VR/panorama camera mode */
    vectNd_copy(&cam->localX, &cam->dirX);
    vectNd_copy(&cam->localY, &cam->dirY);
    vectNd_sub(&cam->imgOrig, &cam->pos, &cam->localZ);
    vectNd_unitize(&cam->localX);
    vectNd_unitize(&cam->localY);
    vectNd_unitize(&cam->localZ);
    cam->prepared = 1;

    vectNd_free(&posX);
    vectNd_free(&posY);

    if( flip_x )
        camera_flip_x(cam);
    if( flip_y )
        camera_flip_y(cam);
    if( zoom != 1.0 )
        camera_zoom(cam);

    return 1;
}

int camera_set_aim(camera *cam, vectNd *pos, vectNd *target, vectNd *up, double rot) {
    /* reset camera to default location and orientation */
    camera_reset(cam);

    vectNd_copy(&cam->viewPoint, pos);
    vectNd_copy(&cam->viewTarget, target);
    if( up )
        vectNd_copy(&cam->up, up);
    cam->rotation = rot;
    cam->eye_offset = EYE_OFFSET;

    return 0;
}

int camera_set_zoom(camera *cam, double zoom) {
    /* This is not a typical zoom factor, it will simply adjust the magnitude
     * of the vectors used to compute virtual pixels. */
    cam->zoom = zoom;

    return 0;
}

int camera_set_flip(camera *cam, int x, int y) {
    cam->flip_x = x;
    cam->flip_y = y;

    return 0;
}

int camera_focus(camera *cam, vectNd *point) {
    /* compute focal distance for specified point */
    vectNd temp;
    vectNd_alloc(&temp,point->n);

    /* get vector from camera, to point */
    vectNd_sub(point,&cam->pos,&temp);

    /* project onto camera's z vector */
    vectNd_proj(&temp,&cam->localZ,&temp);

    /* determine distance to put point in focal plane */
    vectNd_l2norm(&temp,&cam->focal_distance);

    /* cleanup */
    vectNd_free(&temp);

    return 0;
}

int camera_focus_multi(camera *cam, vectNd *points, int n, double near_padding, double far_padding, double confusion_radius, double img_plane_dist) {
    if( points==NULL || n < 1 ) {
        fprintf(stderr,"%s: insufficient points to pick focus parameters.\n", __FUNCTION__);
        return -1;
    }

    /* find nearest and farthest points */
    double min_dist=1, max_dist=-1;
    vectNd_dist(&points[0],&cam->viewPoint,&min_dist);
    max_dist = min_dist;
    for(int i=1; i<n; ++i) {
        double dist = -1;
        vectNd_dist(&points[i],&cam->viewPoint,&dist);
        if( dist > max_dist ) max_dist = dist;
        if( dist < min_dist ) min_dist = dist;
    }
    max_dist += far_padding;
    min_dist -= near_padding;

    /* start radius size way too large, and use binary search */
    double min_radius = 0.0;
    double max_radius = 1/EPSILON;

    /* get distance from pixel plane (image plane) eye point (lens) */
    if( img_plane_dist < 0.0 )
        vectNd_dist(&cam->pos,&cam->imgOrig,&img_plane_dist);

    double u1, u2, v1, v2, f;
    do {
        double curr_radius = (min_radius + max_radius) / 2.0;

        /* goal, find largest aperture radius, s.t. min_dist and max_dist can
         * both be in focus, and the corresponding focal distance */

        /* assume pixels are behind the lens as shown in Figure 10.2 of 
         * Ray Tracing From the Ground Up (p.169). */

        /* find image plane distances on each side of the image plane where the
         * circle of confusion is exactly one pixel */
        /* aperture_radius:confusion_radius = img_plane_dist:conf_dist */
        double conf_dist = (img_plane_dist * confusion_radius) / curr_radius;
        double min_img_dist = img_plane_dist - conf_dist;
        double max_img_dist = img_plane_dist + conf_dist;

        /* find f using 1/f = 1/u + 1/v
         * see: https://en.wikipedia.org/wiki/Focal_length#Thin_lens_approximation
         */
        /* adapt equation to be:
         * 1/f = 1/2 ( 1/u1 + 1/v1 + 1/u2 + 1/v2 )
         * u1 and u2 are min_dist and max_dist
         * v1 and v2 are based on aperture size and image plane distance
         */
        u1 = min_dist;
        u2 = max_dist;
        v1 = min_img_dist;
        v2 = max_img_dist;
        f = 2.0 / (1/u1 + 1/v1 + 1/u2 + 1/v2);

        /* find v1 and v2 based on computed f */
        v1 = 1.0 / (1/f - 1/u1);
        v2 = 1.0 / (1/f - 1/u2);

        /* determine the range that is 'in focus' */
        u1 = 1.0 / (1/f - 1/min_img_dist);
        u2 = 1.0 / (1/f - 1/max_img_dist);

        /* check so see if near and far points are in focus with current aperture size */
        /* i.e. are new v1 and v2 within {min,max}_img_dist range? */
        if( u2 < (min_dist-EPSILON) && u1 > (max_dist+EPSILON) ) {
            /* in focus, so aperture is small enough */
            min_radius = curr_radius;
        } else {
            /* out of focus, so aperture is too big */
            max_radius = curr_radius;
        }

        cam->aperture_radius = curr_radius;
        cam->focal_distance = 1.0 / (1/f - 1/img_plane_dist);

        if( max_radius - min_radius <= pow(EPSILON,2.0) ) {
            printf("%s:\n", __FUNCTION__);
            printf("\tconfusion radius: %g (1/%g)\n", confusion_radius, 1.0/confusion_radius);
            printf("\timage plane distance: %g\n", img_plane_dist);
            printf("\taperture: %.16g; focal distance: %g\n", cam->aperture_radius, cam->focal_distance);
            printf("\tf: %.16g\n", f);
            printf("\tIn focus from %g to %g.\n", u2, u1);
            printf("\tRequested in focus range from %g to %g.\n", min_dist, max_dist);
        }
    } while( max_radius - min_radius > pow(EPSILON,2.0) );

    if( u2 > min_dist || u1 < max_dist ) {
        printf("\n\n");
        printf("\%s: Unable to find valid aperture size for requested focal range, try adjusting image plane distance (currently %g).", __FUNCTION__, img_plane_dist);
        printf("achieved range: %.10g,%.10g, requested: %.10g,%.10g\n",
                min_dist,max_dist, u2,u1);
        printf("u2-min_dist: %g\n", u2-min_dist);
        printf("max_dist-u1: %g\n", max_dist-u1);
        printf("\n\n");
    }

    return 0;
}

void camera_flip_x(camera *cam) {
    vectNd_scale(&cam->dirX,-1,&cam->dirX);

    /* swap eye positions */
    vectNd temp;
    vectNd_calloc(&temp,cam->leftEye.n);
    vectNd_copy(&temp,&cam->leftEye);
    vectNd_copy(&cam->leftEye,&cam->rightEye);
    vectNd_copy(&cam->rightEye,&temp);
    vectNd_free(&temp);
}

void camera_flip_y(camera *cam) {
    vectNd_scale(&cam->dirY,-1,&cam->dirY);
}

void camera_zoom(camera *cam) {
    if( fabs(cam->zoom) < EPSILON )
        return;
    vectNd_scale(&cam->dirX, 1/cam->zoom, &cam->dirX);
    vectNd_scale(&cam->dirY, 1/cam->zoom, &cam->dirY);
}

void camera_target_point(camera *cam, double x, double y, double dist, vectNd *pixel) {
    vectNd temp;
    vectNd_alloc(&temp,pixel->n);
    if( cam->type == CAMERA_VR ) {
        /* compute point in virtual spherical screen */
        double azi = x * cam->hFov;
        double alt = y * cam->vFov;

        /* convert spherical coordinates to camera local x,y,z */
        double view_x = dist * sin(azi) * cos(alt);
        double view_y = dist * sin(alt);
        double view_z = dist * cos(azi) * cos(alt);

        /* start pixel at camera location */
        vectNd_copy(pixel, &cam->pos);

        /* scale local vectors and move pixel to actual location */
        vectNd_scale(&cam->localX, view_x, &temp);
        vectNd_add(pixel, &temp, pixel);
        vectNd_scale(&cam->localY, view_y, &temp);
        vectNd_add(pixel, &temp, pixel);
        vectNd_scale(&cam->localZ, view_z, &temp);
        vectNd_add(pixel, &temp, pixel);

    } else if( cam->type == CAMERA_PANO ) {
        /* compute point in virtual cuylindrical screen */
        /* see:
         * https://facebook360.fb.com/editing-360-photos-injecting-metadata/
         */
        double azi = x * cam->hFov;

        /* set basic cylinder parameters */
        /* Note: these are invarient and should probably be computed
         * elsewhere */
        double y_size = 2.0*tan(cam->vFov/2.0) * dist;

        /* convert spherical coordinates to camera local x,y,z */
        double view_x = dist * sin(azi);
        double view_y = y * y_size;
        double view_z = dist * cos(azi);

        /* start pixel at camera location */
        vectNd_copy(pixel, &cam->pos);

        /* scale local vectors and move pixel to actual location */
        vectNd_scale(&cam->localX, view_x, &temp);
        vectNd_add(pixel, &temp, pixel);
        vectNd_scale(&cam->localY, view_y, &temp);
        vectNd_add(pixel, &temp, pixel);
        vectNd_scale(&cam->localZ, view_z, &temp);
        vectNd_add(pixel, &temp, pixel);
    } else if( cam->type == CAMERA_NORMAL ) {
        /* compute point in virtual planar screen */
        vectNd_copy(pixel,&cam->imgOrig);
        vectNd_scale(&cam->dirX,x,&temp);
        vectNd_add(pixel,&temp,pixel);
        vectNd_scale(&cam->dirY,y,&temp);
        vectNd_add(pixel,&temp,pixel);

        /* project ray onto focal plane */
        double screen_dist = -1;
        vectNd_dist(&cam->imgOrig,&cam->pos,&screen_dist);
        if( screen_dist > EPSILON ) {
            double len;
            vectNd_sub(pixel,&cam->pos,&temp);
            vectNd_l2norm(&temp,&len);

            vectNd_scale(&temp,dist/screen_dist,&temp);
            vectNd_add(&cam->pos,&temp,pixel);
        }
    } else {
        fprintf(stderr,"Unknown camera type: %i\n", cam->type);
    }

    vectNd_free(&temp);
}

void camera_print(camera *cam)
{
    printf("Camera points:\n");
    vectNd_print(&cam->viewPoint,"\tviewPoint");
    vectNd_print(&cam->viewTarget,"\tviewTarget");
    vectNd_print(&cam->up,"\tup");
    if( cam->rotation != 0 )
        printf("\trotation: %g\n", cam->rotation);
    if( cam->eye_offset != EYE_OFFSET )
        printf("\teye_offset: %g\n", cam->eye_offset);
    if( cam->aperture_radius > 0 ) {
        printf("\taperture radius: %g\n", cam->aperture_radius);
        printf("\tfocal distance: %g\n", cam->focal_distance);
    }

    vectNd_print(&cam->pos,"\tposition");
    vectNd_print(&cam->leftEye,"\tleft eye");
    vectNd_print(&cam->rightEye,"\tright eye");
    vectNd_print(&cam->imgOrig,"\timage origin");
    vectNd_print(&cam->dirX,"\timg X");
    vectNd_print(&cam->dirY,"\timg Y");
    vectNd_print(&cam->localX,"\tlocal X");
    vectNd_print(&cam->localY,"\tlocal Y");
    vectNd_print(&cam->localZ,"\tlocal Z");
}
