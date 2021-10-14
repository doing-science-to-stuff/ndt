/*
 * balls.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include <stdio.h>
#ifdef WITH_YAML
#include <yaml.h>
#else
#warning "Not compiled with YAML support."
#endif /* WITH_YAML */
#include "../scene.h"

int scene_frames(int dimensions, char *config) {
    char *fname = config;
    if( fname == NULL )
        return 0;
    if( dimensions < 3 )
        return 0;
    if( config == NULL )
        printf("config string omitted.\n");
    #ifdef WITH_YAML
    return scene_yaml_count_frames(fname);
    #else
    return 1;
    #endif /* WITH_YAML */
}

#ifdef WITH_YAML
int scene_setup(scene *scn, int dimensions, int frame, int frames, char *config)
{
    scene_init(scn, "nameless", dimensions);

    char *fname = config;
    if( fname == NULL ) {
        fprintf(stderr, "%s: YAML scene requires a filename, use `-u filename`.\n", __FUNCTION__);
        exit(1);
    }
    if( frames <= 0 )
        printf("frames is only %i.\n", frames);
    if( config == NULL )
        printf("config string omitted.\n");

    scene_read_yaml(scn, fname, frame);

    return 0;
}
#else /* WITH_YAML */
int scene_setup(scene *scn, int dimensions, int frame, int frames, char *config)
{
    fprintf(stderr, "\n\nNot compiled with YAML support.\n");
    fprintf(stderr, "Make sure libyaml is installed.\n");
    fprintf(stderr, "Recompile using -DWITH_YAML, to enable YAML support.\n\n");
    exit(1);
}
#endif /* WITH_YAML */
