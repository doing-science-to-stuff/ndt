/*
 * camera.h
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */

#ifndef CAMERA_H
#define CAMERA_H

#define EYE_OFFSET 0.125

/* see:
 * https://stackoverflow.com/questions/9907160/how-to-convert-enum-names-to-string-in-c
 */
#define FOREACH_CAMERA_TYPE(CAMERA_TYPE) \
    CAMERA_TYPE(CAMERA_NORMAL)   /* planar 'screen' */ \
    CAMERA_TYPE(CAMERA_VR)     /* spherical 'screen' */ \
    CAMERA_TYPE(CAMERA_PANO)  /* cylindrical 'screen' */

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

typedef enum CAMERA_TYPE_ENUM {
    FOREACH_CAMERA_TYPE(GENERATE_ENUM)
} camera_type_t;

extern const char *CAMERA_TYPE_STRING[];

/* camera is just a set of points and vectors that will start positioned near
 * the origin and moved into place via a series of affine transformations */
typedef struct camera_t
{
    camera_type_t type;

    /* camera aiming input parameters */
    vectNd viewPoint;
    vectNd viewTarget;
    vectNd up;
    double rotation;
    double eye_offset;

    /* depth of field input parameters */
    double aperture_radius;
    double focal_distance;

    /* view modifiers */
    double zoom;
    unsigned int flip_x:1;
    unsigned int flip_y:1;
    unsigned int flatten:1;

    /* parameters for VR/panorama renderings */
    double hFov;
    double vFov;

    /* all variables below are set during aiming */
    unsigned int prepared:1;

    /* location of the actual camera */
    double leveling;    /* rotation needed to level camera */
    vectNd pos;  /* initially origin */
    vectNd leftEye; 
    vectNd rightEye; 

    /* specify the mapping of pixels */
    vectNd dirX;  /* initially origin + <v_0=1> */
    vectNd dirY;  /* initially origin + <v_1=1> */
    vectNd imgOrig; /* initially origin + <v_2=1> */

    /* these unit vectors get set during aiming */
    vectNd localX;
    vectNd localY;
    vectNd localZ;
} camera;

int camera_alloc(camera *cam, int dim);
int camera_free(camera *cam);
int camera_init(camera *cam);
int camera_reset(camera *cam);
int camera_set_aim(camera *cam, vectNd *pos, vectNd *target, vectNd *up, double rot);
int camera_set_zoom(camera *cam, double zoom);
int camera_set_flip(camera *cam, int x, int y);
int camera_aim_naive(camera *cam);
int camera_aim(camera *cam);
int camera_focus(camera *cam, vectNd *point);
int camera_focus_multi(camera *cam, vectNd *points, int n, double near_padding, double far_padding, double confusion_radius, double img_plane_dist);
void camera_target_point(camera *cam, double x, double y, double dist, vectNd *p);
void camera_print(camera *cam);
void camera_flip_x(camera *cam);
void camera_flip_y(camera *cam);
void camera_zoom(camera *cam);

#endif /* CAMERA_H */
