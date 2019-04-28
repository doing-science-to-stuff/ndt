/*
 * image.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef WITH_JPEG
#include <jpeglib.h>
#endif /* WITH_JPEG */
#ifdef WITH_PNG
#include <png.h>
#endif /* WITH_PNG */
#include <jerror.h>
#include <setjmp.h>
#include <pthread.h>
#include <math.h>
#include <zlib.h>
#include "image.h"

int image_init(image_t *img)
{
    memset(img,'\0',sizeof(image_t));
    img->pixel_width = sizeof(pixel_t);
    return 0;
}

int image_set_size(image_t *img, int width, int height)
{
    if( img->pixels ) {
        free(img->pixels); img->pixels=NULL;
    }
    img->pixels = calloc(width*height,img->pixel_width);
    img->allocated = width*height*img->pixel_width;
    img->width = width;
    img->height = height;

    return 0;
}

int image_set_format(image_t *img, image_type type)
{
    if( img==NULL )
        return -1;
    img->type = type;

    return 0;
}

static inline int get_pixel_offset(image_t *img, int x, int y) {
    int pos=0;
    int width = img->width;
    int height = img->height;

    if( x < 0 || y < 0 || x >= width || y >= height ) {
        if( img->edge_style == IMG_EDGE_FLAT ) {
            return -1;
        } else if( img->edge_style == IMG_EDGE_LOOP) {
            if( x < 0 )
                x = width - ((-x) % width);
            if( y < 0 )
                y = height - ((-y) % height);
            if( x >= img->width )
                x = x % width;
            if( y >= img->height )
                y = y % height;
        } else {
            printf("Unsupported image edge style.\n");
        }
    }

    pos = (width*y+x)*img->pixel_width;
    if( pos<0 || pos>=img->allocated ) {
        return -1;
    }

    return pos;
}

int image_set_pixel(image_t *img, int x, int y, pixel_t *color)
{
    int pos=0;
    if( (pos = get_pixel_offset(img,x,y)) < 0 )
        return -1;

    if( img->pixel_width==sizeof(dbl_pixel_t) ) {
        dbl_pixel_t clr;
        /* convert quadractic chars to linear doubles */
        pixel_c2d(clr,*color);
        memcpy(img->pixels+pos,&clr,img->pixel_width);
    } else if( img->pixel_width==sizeof(pixel_t) ) {
        memcpy(img->pixels+pos,color,img->pixel_width);
    } else {
        fprintf(stderr,"%s: unknown pixel width %i.\n", __FUNCTION__, img->pixel_width);
        return -1;
    }

    return 0;
}

int image_get_pixel(image_t *img, int x, int y, pixel_t *color)
{
    int pos=0;

    if( (pos = get_pixel_offset(img,x,y)) < 0 )
        return -1;

    if( img->pixel_width==sizeof(dbl_pixel_t) ) {
        dbl_pixel_t clr;
        memcpy(&clr,img->pixels+pos,img->pixel_width);
        /* convert doubles to chars */
        pixel_d2c(*color,clr);
    } else if( img->pixel_width==sizeof(pixel_t) ) {
        memcpy(color,img->pixels+pos,img->pixel_width);
    } else {
        fprintf(stderr,"%s: unknown pixel width %i.\n", __FUNCTION__, img->pixel_width);
        return -1;
    } 

    return 0;
}


int dbl_image_set_pixel(image_t *img, int x, int y, dbl_pixel_t *color)
{
    int pos=0;

    if( (pos = get_pixel_offset(img,x,y)) < 0 )
        return -1;

    if( img->pixel_width==sizeof(dbl_pixel_t) ) {
        memcpy(img->pixels+pos,color,img->pixel_width);
    } else if( img->pixel_width==sizeof(pixel_t) ) {
        pixel_t clr;
        /* convert linear doubles to quadratic chars */
        pixel_d2c(clr,*color);
        memcpy(img->pixels+pos,&clr,img->pixel_width);
    } else {
        fprintf(stderr,"%s: unknown pixel width %i.\n", __FUNCTION__, img->pixel_width);
        return -1;
    } 

    return 0;
}

int dbl_image_get_pixel(image_t *img, int x, int y, dbl_pixel_t *color)
{
    int pos=0;

    if( (pos = get_pixel_offset(img,x,y)) < 0 ) {
        memset(color,'\0',sizeof(*color));
        return -1;
    }

    if( img->pixel_width==sizeof(dbl_pixel_t) ) {
        memcpy(color,img->pixels+pos,img->pixel_width);
    } else if( img->pixel_width==sizeof(pixel_t) ) {
        pixel_t clr;
        memcpy(&clr,img->pixels+pos,img->pixel_width);
        /* convert quadratic chars to linear doubles */
        pixel_c2d(*color,clr);
    } else {
        fprintf(stderr,"%s: unknown pixel width %i.\n", __FUNCTION__, img->pixel_width);
        return -1;
    } 

    return 0;
}

int image_convolve(image_t *dst, image_t *src, matrix_t *mtx)
{
    int i,j,k,l;
    int cx, cy;

    if( src->width != dst->width || src->height != dst->height )
    {
        fprintf(stderr,"%s: destination image is incompatible size. (%i,%i) "
                        "vs. (%i,%i)\n", __FUNCTION__,
                        dst->width, dst->height, src->width, src->height);
        return -1;
    }

    if( (mtx->rows % 2)==0 || (mtx->cols % 2)==0 )
    {
        fprintf(stderr,"%s: convolution matrix must have odd dimensions.",
                __FUNCTION__);
        return -1;
    }

    cx = mtx->cols/2;
    cy = mtx->rows/2;
    for(j=0; j<src->height; ++j)
    {
        for(i=0; i<src->width; ++i)
        {
            double sum_r=0.0;
            double sum_g=0.0;
            double sum_b=0.0;
            double sum_a=0.0;
            dbl_pixel_t src_pixel;
            double mtxval = 0.0;

            /* compute destination pixel */
            for(k=0; k<mtx->rows; ++k)
            {
                for(l=0; l<mtx->cols; ++l)
                {
                    if( (i+l-cx) > 0 && (j+k-cy) > 0 
                        && (i+l-cx) < src->width && (j+k-cy) < src->height ) 
                    {
                        dbl_image_get_pixel(src,i+l-cx,j+k-cy,&src_pixel);
                        mtxval = matrix_get_value(mtx,k,l);
                        sum_r += mtxval*src_pixel.r;
                        sum_g += mtxval*src_pixel.g;
                        sum_b += mtxval*src_pixel.b;
                        sum_a += mtxval*src_pixel.a;
                    }
                }
            }

            dbl_pixel_t dst_pixel;
            dst_pixel.r = sum_r;
            dst_pixel.g = sum_g;
            dst_pixel.b = sum_b;
            dst_pixel.a = sum_a;
            dbl_image_set_pixel(dst,i,j,&dst_pixel);
        }
    }

    return 0;
}

int image_greyscale(image_t *img)
{
    int i,j,g;
    pixel_t p;

    for(j=0;j<img->height;++j)
    {
        for(i=0;i<img->width;++i)
        {
            g = -1;
            image_get_pixel(img,i,j,&p);
            g = MAX(MAX(p.r, p.g), p.b);
            p.r = g; p.g = g; p.b = g;
            image_set_pixel(img,i,j,&p);
        }
    }

    return 0;
}

int image_copy(image_t *dst, image_t *src)
{
    if( src->pixel_width == sizeof(dbl_pixel_t) )
        dbl_image_init(dst);
    else
        image_init(dst);
    image_set_size(dst,src->width,src->height);
    if( dst->pixel_width == src->pixel_width ) {
        memcpy(dst->pixels,src->pixels,src->allocated);
    } else {
        fprintf(stderr,"%s: copying to different pixel width (%i != %i) not implemented yet!\n", __FUNCTION__, dst->pixel_width, src->pixel_width);
    }

    return 0;
}

static int image_get_jpeg_comment(image_t *img, j_decompress_ptr cinfo)
{
    return 0;
}

static int image_load_jpeg(image_t *img, char *fname)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr error_mgr;
    jmp_buf setjmp_buffer;
    FILE * infile;
    JSAMPARRAY buffer;
    int row_stride;

    /* initialize image structure */
    image_init(img);

    /* open file */
    if( (infile=fopen(fname,"rb")) == NULL )
    {
        perror("fopen");
        return -1;
    }

    printf("Loading JPEG image from '%s'.\n", fname);

    /* setup error handler */
    if( setjmp(setjmp_buffer) )
    {
        jpeg_destroy_decompress(&cinfo);
        fclose(infile); infile=NULL;
    }

    /* setup jpeg decoder */
    cinfo.err = jpeg_std_error(&error_mgr);
    jpeg_create_decompress(&cinfo);
    jpeg_save_markers(&cinfo, JPEG_COM, 0xFFFF); /* request comments */
    jpeg_stdio_src(&cinfo, infile);
    cinfo.out_color_space = JCS_RGB;
    jpeg_read_header(&cinfo, FALSE);

    /* print markers */
    image_get_jpeg_comment(img,&cinfo);
    if( img->comment )
        printf("Comment: %s\n", img->comment);

    /* start decoding */
    jpeg_start_decompress(&cinfo);

    /* allocate image */
    image_set_size(img,cinfo.output_width,cinfo.output_height);

    /* read image data */
    row_stride = cinfo.output_width * cinfo.output_components;
    buffer = (*cinfo.mem->alloc_sarray)
                ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);
    while( cinfo.output_scanline < cinfo.output_height )
    {
        int i=0;

        jpeg_read_scanlines(&cinfo, buffer, 1);

        for(i=0; i<cinfo.output_width; ++i)
        {
            pixel_t clr;

            clr.r = buffer[0][cinfo.output_components*i];
            clr.g = buffer[0][cinfo.output_components*i+1];
            clr.b = buffer[0][cinfo.output_components*i+2];
            clr.a = 255;

            image_set_pixel(img,i,cinfo.output_scanline-1,&clr);
        }
    }

    /* finish reading data */
    jpeg_finish_decompress(&cinfo);

    /* cleanup */
    jpeg_destroy_decompress(&cinfo);
    fclose(infile); infile=NULL;

    return 0;
}

static int image_save_jpeg(image_t *img, char *fname)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE * outfile;
    JSAMPROW row_pointer[1];
    JSAMPLE *rgb_row=NULL;

    printf("Writing: %s\n", fname);
    /* initialize jpeg stuff */
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    /* open output file */
    if( (outfile=fopen(fname,"wb")) == NULL)
    {
        perror("fopen");
        return -1;
    }
    jpeg_stdio_dest(&cinfo, outfile);

    /* setup jpeg parameters */
    cinfo.image_width = img->width;
    cinfo.image_height = img->height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);

    /* range of quality is 0 to 100 */
    jpeg_set_quality(&cinfo, 95, TRUE);

    /* start writing the file */
    jpeg_start_compress(&cinfo, TRUE);

    rgb_row = calloc(img->width,cinfo.input_components*sizeof(JSAMPLE));
    row_pointer[0] = &rgb_row[0];
    while (cinfo.next_scanline < cinfo.image_height)
    {
        int i=0;

        /* copy RGB components into a temp buffer */
        for(i=0;i<img->width;++i)
        {
            pixel_t clr;
            memset(&clr, '\0', sizeof(clr));

            image_get_pixel(img,i,cinfo.next_scanline,&clr);

            rgb_row[i*3]   = clr.r;
            rgb_row[i*3+1] = clr.g;
            rgb_row[i*3+2] = clr.b;
        }
        row_pointer[0] = &rgb_row[0];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    free(rgb_row); rgb_row=NULL;

    /* close file */
    fclose(outfile); outfile=NULL;

    /* cleanup */
    jpeg_destroy_compress(&cinfo);

    return 0;
}

#ifdef WITH_PNG
static int image_load_png(image_t *img, char *fname)
{
    png_infop end_info=NULL;
    png_voidp user_error_ptr=NULL;
    png_error_ptr user_error_fn=NULL;
    png_error_ptr user_warning_fn=NULL;
    png_uint_32 width=0, height=0;
    int color_type=0;
    unsigned int i=0, j=0, pixel_width=1;

    /* This is all 'based on' the libpng(3) manpage */

    /* Setup */
    FILE *fp = fopen(fname, "rb");
    if (!fp)
    {
        perror(fname);
        return -1;
    }

    png_bytep header;
    png_size_t num_to_check=8;
    header = calloc(1, num_to_check*sizeof(png_byte));

    fread(header, 1, num_to_check, fp);
    int is_png = !png_sig_cmp(header, 0, num_to_check);
    free(header);   header=NULL;

    if (!is_png)
    {
        printf("%s: Not a PNG file\n", fname);
        fclose(fp); fp=NULL;
        return -2;
    }

    png_structp png_ptr = png_create_read_struct
        (PNG_LIBPNG_VER_STRING, (png_voidp)user_error_ptr,
         user_error_fn, user_warning_fn);

    if (!png_ptr) {
        fclose(fp); fp=NULL;
        return -3;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);

    if (!info_ptr)
    {
        png_destroy_read_struct(&png_ptr,
                (png_infopp)NULL, (png_infopp)NULL);
        fclose(fp); fp=NULL;
        return -4;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_read_struct(&png_ptr, &info_ptr,
                &end_info);
        fclose(fp); fp=NULL;
        return -5;
    }

    png_init_io(png_ptr, fp);

    png_set_sig_bytes(png_ptr, num_to_check);

    png_set_crc_action(png_ptr, PNG_CRC_WARN_USE, PNG_CRC_WARN_USE);

    /* Setting up callback code */
    
    /* Unknown-chunk handling */

    /* User limits */
    png_uint_32 width_max = 0x7fffffffL;
    png_uint_32 height_max = 0x7fffffffL;
    png_set_user_limits(png_ptr, width_max, height_max);

    /* Information about your system */

    /* The high-level read interface */
    int png_transforms=PNG_TRANSFORM_IDENTITY;
    png_transforms = PNG_TRANSFORM_GRAY_TO_RGB|PNG_TRANSFORM_PACKING;
    png_read_png(png_ptr, info_ptr, png_transforms, NULL);

    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);

    printf("color_type=");
    switch( color_type )
    {
        case PNG_COLOR_TYPE_GRAY:
            printf("PNG_COLOR_TYPE_GRAY\n");
            pixel_width = 1;
            break;
        case PNG_COLOR_TYPE_GRAY_ALPHA:
            printf("PNG_COLOR_TYPE_GRAY_ALPHA\n");
            pixel_width = 2;
            break;
        case PNG_COLOR_TYPE_PALETTE:
            printf("PNG_COLOR_TYPE_PALETTE\n");
            pixel_width = 1;
            break;
        case PNG_COLOR_TYPE_RGB:
            printf("PNG_COLOR_TYPE_RGB\n");
            pixel_width = 3;
            break;
        case PNG_COLOR_TYPE_RGB_ALPHA:
            printf("PNG_COLOR_TYPE_RGB_ALPHA\n");
            pixel_width = 4;
            break;
        default:
            printf("Unknown.\n");
            break;
    }

    png_bytepp row_pointers = png_get_rows(png_ptr, info_ptr);
    if( row_pointers==NULL ) {
        printf("png_get_rows: returned NULL\n");
        fclose(fp); fp=NULL;
        return -6;
    }
    
    image_set_size(img,width,height);
    for(j=0; j<height; j++)
    {
        for(i=0; i<width; i++)
        {
            pixel_t p;
            p.r = row_pointers[j][pixel_width*i+0];
            p.g = row_pointers[j][pixel_width*i+1];
            p.b = row_pointers[j][pixel_width*i+2];
            p.a = row_pointers[j][pixel_width*i+3];
            if( pixel_width >= 4 )
                p.a = row_pointers[j][pixel_width*i+3];
            image_set_pixel(img,i,j,&p);
        }
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
    fclose(fp); fp=NULL;

    return 0;
}

static int image_save_png(image_t *img, char *fname)
{
    /* based on man libpng for libpng 1.6.15 */
    void *user_error_ptr = NULL;
    void *user_error_fn = NULL;
    void *user_warning_fn = NULL;

    /* Setup */
    FILE *fp = fopen(fname, "wb");
    if (!fp) {
        perror(fname);
        return (-1);
    }

    png_structp png_ptr = png_create_write_struct
        (PNG_LIBPNG_VER_STRING, (png_voidp)user_error_ptr,
         user_error_fn, user_warning_fn);

    if (!png_ptr) {
        fprintf(stderr,"png_create_write_struct returned %p\n", png_ptr);
        fclose(fp); fp=NULL;
        return (-1);
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        fprintf(stderr,"png_create_info_struct returned %p\n", png_ptr);
        png_destroy_write_struct(&png_ptr,
                (png_infopp)NULL);
        fclose(fp); fp=NULL;
        return (-1);
    }

    /* error handling */
#if 1
    if (setjmp(png_jmpbuf(png_ptr)))
    {
        fprintf(stderr,"setjmp failed\n");
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp); fp=NULL;
        return (-1);
    }
#endif /* 0 */

    #if 0
    png_set_check_for_invalid_index(png_ptr, 0);
    #endif /* 0 */

    png_init_io(png_ptr, fp);

    #if 0
    /* as far as I can tell, this is optional */
    png_set_write_status_fn(png_ptr, write_row_callback);
    #endif /* 0 */

    /* Set the zlib compression level */
    png_set_compression_level(png_ptr,
            Z_BEST_COMPRESSION);

    /* Set other zlib parameters for compressing IDAT */
    png_set_compression_mem_level(png_ptr, 8);
    png_set_compression_strategy(png_ptr,
            Z_DEFAULT_STRATEGY);
    png_set_compression_window_bits(png_ptr, 15);
    png_set_compression_method(png_ptr, 8);
    png_set_compression_buffer_size(png_ptr, 8192);

    /* Setting the contents of info for output */
    png_set_IHDR(png_ptr, info_ptr, img->width, img->height,
            8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    /* The high-level write interface */
    png_byte *row_pointers[img->height];
    for (int j=0; j<img->height; j++)
    {
        int pixel_size=4;
        row_pointers[j]=png_malloc(png_ptr, img->width*pixel_size);

        for(int i=0;i<img->width;i++)
        {
            pixel_t clr;
            memset(&clr, '\0', sizeof(clr));
            image_get_pixel(img,i,j,&clr);
            row_pointers[j][pixel_size*i+0] = clr.r;
            row_pointers[j][pixel_size*i+1] = clr.g;
            row_pointers[j][pixel_size*i+2] = clr.b;
            row_pointers[j][pixel_size*i+3] = clr.a;
        }
    }
    png_set_rows(png_ptr, info_ptr, row_pointers);

    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    /* If  you  use png_set_rows(), the application is responsible for freeing
     * row_pointers (and row_pointers[i], if they were separately allocated).
     */
    for (int j=0; j<img->height; j++)
    {
        png_free(png_ptr, row_pointers[j]);
    }

    #if 0
    /* Writing the image data */
    png_write_image(png_ptr, row_pointers);
    #endif /* 0 */

    /* Finishing a sequential write */
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);

    fclose(fp); fp=NULL;

    return 0;
}
#endif /* WITH_PNG */

int image_load(image_t *img, char *fname, int format)
{
    switch(format) {
        #ifdef WITH_PNG
        case IMG_TYPE_PNG:
            return image_load_png(img,fname);
            break;
        #endif /* WITH_PNG */
        #ifdef WITH_JPEG
        case IMG_TYPE_JPEG:
            return image_load_jpeg(img,fname);
            break;
        #endif /* WITH_PNG */
        default:
        case IMG_TYPE_UNKNOWN:
            fprintf(stderr, "%s: Unknown image type specifier %i\n", __FUNCTION__, format);
            return -1;
            break;
    }

    return 0;
}

int image_save(image_t *img, char *fname, int format)
{
    unlink(fname);
    switch(format) {
        #ifdef WITH_PNG
        case IMG_TYPE_PNG:
            return image_save_png(img,fname);
            break;
        #endif /* WITH_PNG */
        #ifdef WITH_JPEG
        case IMG_TYPE_JPEG:
            return image_save_jpeg(img,fname);
            break;
        #endif /* WITH_PNG */
        default:
        case IMG_TYPE_UNKNOWN:
            fprintf(stderr, "%s: Unknown image type specifier %i\n", __FUNCTION__, format);
            return -1;
            break;
    }

    return 0;
}

int image_save_time(image_t *img, char *fname, int format, struct timeval mtime)
{
    struct timeval times[2];

    image_save(img,fname,format);
    times[0] = mtime;
    times[1] = mtime;
    utimes(fname,times);

    return 0;
}

typedef struct save_thr_info {
    image_t img;
    char fname[PATH_MAX];
    int format;
} save_thr_info_t;

pthread_mutex_t io_mutex = PTHREAD_MUTEX_INITIALIZER;
static int io_count = 0;

static void *image_save_thread(void *arg)
{
    pthread_mutex_lock(&io_mutex);
    ++io_count;
    pthread_mutex_unlock(&io_mutex);
    save_thr_info_t *info = (save_thr_info_t*)arg;
    image_save(&info->img, info->fname, info->format);
    image_free(&info->img);
    free(info); info=NULL;
    pthread_mutex_lock(&io_mutex);
    --io_count;
    pthread_mutex_unlock(&io_mutex);

    return NULL;
}

int image_save_bg(image_t *img, char *fname, int format)
{
    /* make local copy of image and parameters */
    save_thr_info_t *info;
    info = calloc(1, sizeof(save_thr_info_t));
    image_copy(&info->img, img);
    strncpy(info->fname, fname, sizeof(info->fname));
    info->fname[sizeof(info->fname)-1] = '\0';
    info->format = format;

    /* ensure no conflict with initial save */
    unlink(fname);

    /* launch background thread, that calls image_save */
    pthread_t thr_info;
    memset(&thr_info,'\0',sizeof(thr_info));
    if( pthread_create(&thr_info,NULL,image_save_thread,info) == 0 ) {
        /* detach thread */
        if( pthread_detach(thr_info) != 0 ) {
            fprintf(stderr,"pthread_detach failed, attempting to join.\n");
            pthread_join(thr_info,NULL);
        }
        return 0;
    }
    image_free(&info->img);
    free(info); info = NULL;
    fprintf(stderr,"failed to launch background thread, saving %s in foreground.\n", fname);

    return image_save(img, fname, format);
}

int image_active_saves() {
    int ret = 0;
    pthread_mutex_lock(&io_mutex);
    ret = io_count;
    pthread_mutex_unlock(&io_mutex);
    return ret;
}

int image_free(image_t *img)
{
    if( img->pixels ) {
        free(img->pixels); img->pixels=NULL;
    }
    memset(img,'\0',sizeof(image_t));

    return 0;
}

int image_draw_circle(image_t *img, int x, int y, double radius, pixel_t *clr)
{
    int cx, cy;

    for(cx=0; cx<=radius; ++cx)
    {
        cy = sin(acos(cx/radius))*radius;
        image_set_pixel(img,x+cx,y+cy,clr);
        image_set_pixel(img,x+cx,y-cy,clr);
        image_set_pixel(img,x-cx,y+cy,clr);
        image_set_pixel(img,x-cx,y-cy,clr);

        image_set_pixel(img,x+cy,y+cx,clr);
        image_set_pixel(img,x+cy,y-cx,clr);
        image_set_pixel(img,x-cy,y+cx,clr);
        image_set_pixel(img,x-cy,y-cx,clr);
    }

    return 0;
}

int image_draw_line(image_t *img, int x1, int y1, int x2, int y2, pixel_t *clr)
{
    int xm, ym;

    if( abs(x2-x1) > abs(y2-y1) )
    {
        if( x1 > x2 )
        {
            xm = x1;
            x1 = x2;
            x2 = xm;

            ym = y1;
            y1 = y2;
            y2 = ym;
        }

        /* x is the large direction */
        for(xm=x1; xm<x2; ++xm)
        {
            ym = y1 + (xm-x1) * (y2-y1)/(double)(x2-x1);
            image_set_pixel(img,xm,ym,clr);
        }
    }
    else
    {
        if( y1 > y2 )
        {
            xm = x1;
            x1 = x2;
            x2 = xm;

            ym = y1;
            y1 = y2;
            y2 = ym;
        }

        /* y is the large direction */
        for(ym=y1; ym<y2; ++ym)
        {
            xm = x1 + (ym-y1) * (x2-x1)/(double)(y2-y1);
            image_set_pixel(img,xm,ym,clr);
        }
    }

    return 0;
}

int image_fill_gauss_matrix(matrix_t *gauss,int mat_size,double std_dev)
{
    int i=0, j=0;
    double val=0.0;
    double sum=0.0;

    /* build gaussian convolution matrix */
    matrix_init(gauss,mat_size,mat_size);
    for(i=0; i<mat_size; ++i) {
        for(j=0; j<mat_size; ++j) {
            double x=0, y=0;
            x = i-mat_size/2;
            y = j-mat_size/2;
            val = (1.0/(2.0*M_PI*std_dev*std_dev)) * 
                pow(M_E,-(x*x+y*y)/(2*std_dev*std_dev));
            matrix_set_value(gauss,i,j,val);
            sum += val;
        }
    }

    for(i=0; i<mat_size; ++i) {
        for(j=0; j<mat_size; ++j) {
            matrix_set_value(gauss,i,j,matrix_get_value(gauss,i,j)/sum);
        }
    }

    return 0;
}

int image_subtract(image_t *a, image_t *b, image_t *diff)
{
    int i=0, j=0;
    dbl_pixel_t ap, bp;

    image_set_size(diff,a->width,a->height);
    for(j=0; j<a->height; ++j) {
        for(i=0; i<a->width; ++i) {
            dbl_image_get_pixel(a,i,j,&ap);
            dbl_image_get_pixel(b,i,j,&bp);
            ap.r = fabs(ap.r-bp.r);
            ap.g = fabs(ap.g-bp.g);
            ap.b = fabs(ap.b-bp.b);
            ap.a = fabs(ap.a-bp.a);
            dbl_image_set_pixel(diff,i,j,&ap);
        }
    }

    return 0;
}

int bilinear_pixel(int x1, int y1, int x2, int y2, dbl_pixel_t *s1, dbl_pixel_t *s2, dbl_pixel_t *s3, dbl_pixel_t *s4, double x, double y, dbl_pixel_t *p)
{
    p->r = bilinear(x1,y1,x2,y2, s1->r, s2->r, s3->r, s4->r, x, y);
    p->g = bilinear(x1,y1,x2,y2, s1->g, s2->g, s3->g, s4->g, x, y);
    p->b = bilinear(x1,y1,x2,y2, s1->b, s2->b, s3->b, s4->b, x, y);
    p->a = bilinear(x1,y1,x2,y2, s1->a, s2->a, s3->a, s4->a, x, y);

    return 0;
}

/* see: http://en.wikipedia.org/wiki/Bilinear_interpolation */
double bilinear(int x1, int y1, int x2, int y2,
                double v11, double v21, double v12, double v22,
                double x, double y)
{
    double ret=0.0;
    double div = (x2-x1)*(y2-y1);

    ret = (v11*(x2-x)*(y2-y) + v21*(x-x1)*(y2-y) +
          v12*(x2-x)*(y-y1) + v22*(x-x1)*(y-y1)) / div;

    return ret;
}

int image_scale_bilinear(image_t *dst, image_t *src,
                            double scaleX, double scaleY)
{
    int i=0, j=0;
    dbl_pixel_t s1, s2, s3, s4, dp;

    image_set_size(dst,src->width*scaleX,src->height*scaleY);
    for(j=0; j<dst->height; ++j) {
        for(i=0; i<dst->width; ++i) {
            dbl_image_get_pixel(src,i/scaleX,j/scaleY,&s1);
            dbl_image_get_pixel(src,i/scaleX+1,j/scaleY,&s2);
            dbl_image_get_pixel(src,i/scaleX,j/scaleY+1,&s3);
            dbl_image_get_pixel(src,i/scaleX+1,j/scaleY+1,&s4);

            /* compute bilinear inperpolation of s1,s2,s3, and s4 */
            dp.r = bilinear(i/scaleX,j/scaleY,i/scaleX+1,j/scaleY+1,
                            s1.r, s2.r, s3.r, s4.r, i/scaleX, j/scaleY);
            dp.g = bilinear(i/scaleX,j/scaleY,i/scaleX+1,j/scaleY+1,
                            s1.g, s2.g, s3.g, s4.g, i/scaleX, j/scaleY);
            dp.b = bilinear(i/scaleX,j/scaleY,i/scaleX+1,j/scaleY+1,
                            s1.b, s2.b, s3.b, s4.b, i/scaleX, j/scaleY);
            dp.a = bilinear(i/scaleX,j/scaleY,i/scaleX+1,j/scaleY+1,
                            s1.a, s2.a, s3.a, s4.a, i/scaleX, j/scaleY);

            dbl_image_set_pixel(dst,i,j,&dp);
        }
    }

    return 0;
}

int dbl_image_init(image_t *img)
{
    image_init(img);
    img->pixel_width = sizeof(dbl_pixel_t);
    return 0;
}

static inline double normalize_value(double v, double min, double max) {
    if( min == max )
        return min;
    return (v-min)/(max-min);
}

int dbl_image_normalize(image_t *norm, image_t *dblimg)
{
    dbl_pixel_t clr, min, max;
    dbl_pixel_t c;
    int i=0, numPixels=0;

    image_set_size(norm,dblimg->width,dblimg->height);

    /* find min and max for each colour */
    numPixels = dblimg->width*dblimg->height;
    memcpy(&min,dblimg->pixels,dblimg->pixel_width);
    memcpy(&max,dblimg->pixels,dblimg->pixel_width);
    for(i=0; i<numPixels; ++i) {
        memcpy(&clr,dblimg->pixels+i*dblimg->pixel_width,dblimg->pixel_width);

        /* update min */
        if( clr.r < min.r ) min.r = clr.r;
        if( clr.g < min.g ) min.g = clr.g;
        if( clr.b < min.b ) min.b = clr.b;
        if( clr.a < min.a ) min.b = clr.a;

        /* update max */
        if( clr.r > max.r ) max.r = clr.r;
        if( clr.g > max.g ) max.g = clr.g;
        if( clr.b > max.b ) max.b = clr.b;
        if( clr.a > max.a ) max.a = clr.a;
    }

    /* copy normalized image */
    for(i=0; i<numPixels; ++i) {
        int pos = 0;
        memcpy(&clr,&(((dbl_pixel_t*)dblimg->pixels)[i]),dblimg->pixel_width);
        c.r = normalize_value(clr.r, min.r, max.r);
        c.g = normalize_value(clr.g, min.g, max.g);
        c.b = normalize_value(clr.b, min.b, max.b);
        c.a = normalize_value(clr.a, min.a, max.a);
        pos = i*norm->pixel_width;
        memcpy(norm->pixels+pos,&c,dblimg->pixel_width);
    }

    return 0;
}

int image_rgb2hsv(int r, int g, int b, double *h, double *s, double *v)
{
    int min,  max;
    double max_diff=0.0;

    max = MAX(r,MAX(g,b));
    min = MIN(r,MIN(g,b));

    *v = max;

    max_diff = max-min;
    *s = (max!=0)?(255.0*max_diff/max):0;

    if( max == min ) {
        *h = 0;
    } else {
        if( r==max ) {
            *h = 60.0*(g - b) / max_diff + ((g<b)?360:0);
        } else if( g==max ) {
            *h = 60.0*(b - r) / max_diff + 120;
        } else /* b==max */ {
            *h = 60.0*(r - g) / max_diff + 240;
        }
    }

    return 0;
}

int image_hsv2rgb(double h, double s, double v, int *r, int *g, int *b)
{
    int  h_int;
    double p, q, t;
    double f, sf, vf;

    if( s == 0 )    /* grey */
    {
        *r = *g = *b = v;
    }

    h_int = h / 60;
    f = ((h/60.0) - h_int);
    sf = s / 255.0;
    vf = v / 255.0;
    p = vf * ( 1 - sf );
    q = vf * ( 1 - sf * f );
    t = vf * ( 1 - sf * ( 1 - f ) );

    if( h < 60 )
    {
        *r = vf*255;
        *g = t*255;
        *b = p*255;
    }
    else if( h < 120 )
    {
        *r = q*255;
        *g = vf*255;
        *b = p*255;
    }
    else if( h < 180 )
    {
        *r = p*255;
        *g = vf*255;
        *b = t*255;
    }
    else if( h < 240 )
    {
        *r = p*255;
        *g = q*255;
        *b = vf*255;
    }
    else if( h < 300 )
    {
        *r = t*255;
        *g = p*255;
        *b = vf*255;
    }
    else
    {
        *r = vf*255;
        *g = p*255;
        *b = q*255;
    }

    return 0;
}

int image_avg_pixels4(pixel_t *p1, pixel_t *p2,
                      pixel_t *p3, pixel_t *p4,
                      pixel_t *avg, int *var) {
    dbl_pixel_t dp1, dp2, dp3, dp4;
    dbl_pixel_t davg;
    double dvar;

    /* convert to linear double pixels before averaging */
    pixel_c2d(dp1,*p1);
    pixel_c2d(dp2,*p2);
    pixel_c2d(dp3,*p3);
    pixel_c2d(dp4,*p4);
    image_avg_dbl_pixels4(&dp1,&dp2,&dp3,&dp4,&davg,&dvar);
    pixel_d2c(*avg,davg);
    if( var!=NULL )
        *var = dvar;
    
    return 0;
}

int image_avg_dbl_pixels4(dbl_pixel_t *p1, dbl_pixel_t *p2,
                      dbl_pixel_t *p3, dbl_pixel_t *p4,
                      dbl_pixel_t *avg, double *var) {
    avg->r = (p1->r + p2->r + p3->r + p4->r)/4;
    avg->g = (p1->g + p2->g + p3->g + p4->g)/4;
    avg->b = (p1->b + p2->b + p3->b + p4->b)/4;
    avg->a = (p1->a + p2->a + p3->a + p4->a)/4;

    if( var!=NULL ) {
        double v = 0;
        v += fabs(avg->r-p1->r) + fabs(avg->r-p2->r) + 
             fabs(avg->r-p3->r) + fabs(avg->r-p4->r);
        v += fabs(avg->g-p1->g) + fabs(avg->g-p2->g) + 
             fabs(avg->g-p3->g) + fabs(avg->g-p4->g);
        v += fabs(avg->b-p1->b) + fabs(avg->b-p2->b) + 
             fabs(avg->b-p3->b) + fabs(avg->b-p4->b);
        v += fabs(avg->a-p1->a) + fabs(avg->a-p2->a) + 
             fabs(avg->a-p3->a) + fabs(avg->a-p4->a);
        *var = v;
    }
    
    return 0;
}
