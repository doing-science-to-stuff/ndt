/*
 * image.h
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#ifndef IMAGE_H
#define IMAGE_H
#include <sys/time.h>
#include <math.h>
#include "matrix.h"

/* pixel with integer channel ranges from 0 to 255 */
typedef struct pixel
{
    /* values 255*sqrt(linear value in [0:1]) */
    /* see: https://youtu.be/LKnqECcg6Gw */
    unsigned char r, g, b, a;
} pixel_t;

/* pixel with double channel ranges from 0 to 1 */
typedef struct dbl_pixel
{
    /* values should be linear values in [0:1] */
    double r, g, b, a;
} dbl_pixel_t;

#ifndef MIN
#define MIN(x,y)  (((x)<(y))?(x):(y))
#endif /* MIN */
#ifndef MAX
#define MAX(x,y) (((x)>(y))?(x):(y))
#endif /* MAX */
#define pixel_d2c(c,d) { (c).r = sqrt(MAX(0.0,MIN(1.0,(d).r)))*255; \
                         (c).g = sqrt(MAX(0.0,MIN(1.0,(d).g)))*255; \
                         (c).b = sqrt(MAX(0.0,MIN(1.0,(d).b)))*255; \
                         (c).a = sqrt(MAX(0.0,MIN(1.0,(d).a)))*255; }
#define pixel_c2d(d,c) { (d).r = pow((c).r/255.0,2.0); \
                         (d).g = pow((c).g/255.0,2.0); \
                         (d).b = pow((c).b/255.0,2.0); \
                         (d).a = pow((c).a/255.0,2.0); }

#ifndef IMAGE_FORMAT
#if defined(WITH_PNG)
#define IMAGE_FORMAT IMG_TYPE_PNG
#elif defined(WITH_JPEG)
#define IMAGE_FORMAT IMG_TYPE_JPEG
#else
#error "No image file support (e.g. WITH_PNG or WITH_JPEG)"
#endif
#endif /* IMAGE_FORMAT */

typedef enum image_type_t {
    #ifdef WITH_JPEG
    IMG_TYPE_JPEG,
    #endif /* WITH_JPEG */
    #ifdef WITH_PNG
    IMG_TYPE_PNG,
    #endif /* WITH_PNG */
    IMG_TYPE_UNKNOWN
} image_type;

typedef enum image_edge_type_t {
    IMG_EDGE_FLAT,
    IMG_EDGE_LOOP,
} image_edge_style;

typedef struct image
{
    int width, height;
    int pixel_width;
    int allocated;
    image_type type;
    image_edge_style edge_style;
    void *pixels;
} image_t;

/* image basics */
int image_init(image_t*);
int image_set_size(image_t *img, int x, int y);
int image_set_format(image_t *img, image_type type);
int image_set_pixel(image_t*,int,int,pixel_t*);
int image_get_pixel(image_t*,int,int,pixel_t*);
int image_get_subpixel_bilinear(image_t*,double,double,pixel_t*);
int image_load(image_t*,char*,int);
int image_save(image_t*,char*,int);
int image_save_time(image_t*,char*,int,struct timeval);
int image_save_bg(image_t*,char*,int);
int image_active_saves();
int image_free(image_t *img);

/* image operations */
int image_draw_circle(image_t *img, int x, int y, double radius, pixel_t *clr);
int image_draw_line(image_t *img, int x1, int y1, int x2, int y2, pixel_t *clr);
int image_convolve(image_t*,image_t*,matrix_t*);
int image_greyscale(image_t *img);
int image_fill_gauss_matrix(matrix_t *gauss,int mat_size,double std_dev);
int image_subtract(image_t *a, image_t *b, image_t *diff);
int image_add(image_t *a, image_t *b, image_t *sum);
int bilinear_pixel(int x1, int y1, int x2, int y2, dbl_pixel_t *s1, dbl_pixel_t *s2, dbl_pixel_t *s3, dbl_pixel_t *s4, double x, double y, dbl_pixel_t *p);
double bilinear(int x1, int y1, int x2, int y2, double v11, double v21, double v12, double v22, double x, double y);
int image_scale_bilinear(image_t *dst, image_t *src, double scaleX, double scaleY);
int image_copy(image_t *dst, image_t *src);

/* special double image functions */
int dbl_image_init(image_t*);
int dbl_image_set_pixel(image_t*,int,int,dbl_pixel_t*);
int dbl_image_get_pixel(image_t*,int,int,dbl_pixel_t*);
int dbl_image_normalize(image_t*,image_t*);

/* colour conversion */
int image_rgb2hsv(int r, int g, int b, double *h, double *s, double *v);
int image_hsv2rgb(double h, double s, double v, int *r, int *g, int *b);

/* pixel operations */
int image_avg_pixels4(struct pixel *p1, struct pixel *p2,
                      struct pixel *p3, struct pixel *p4,
                      struct pixel *avg, int *var);

int image_avg_dbl_pixels4(dbl_pixel_t *p1, dbl_pixel_t *p2,
                          dbl_pixel_t *p3, dbl_pixel_t *p4,
                          dbl_pixel_t *avg, double *var);

#endif /* IMAGE_H */
