# NDT: N-Dimensional Tracer

NDT is a hyper-dimensional ray-tracer.
Just like any normal ray-tracer ndt produces images by tracing the path of
light rays from an observer to objects and light sources.
However unlike normal ray-tracers, all of the vectors used to represent light rays can have an arbitrary number of dimension.
This allows for direct rendering of scenes that have more than just three spacial dimensions.

## Getting Started

### Dependancies

Packages for these dependancies should exist for most Linux distributions and
are available via [macports](https://www.macports.org) on macOS.

 * libjpeg and/or libpng
 * libyaml (optional)

In [Debian](https://www.debian.org/) Linux:
```text
$ sudo apt-get install libjpeg-dev libpng-dev libyaml-dev
```

In macOS using [macports](https://www.macports.org):
```text
$ sudo port install jpeg libpng libyaml
```

### Linux or macOS

Build from source:
```text
$ cd ndt
$ make
```
*Note: If not all dependencies are met, manual editing of `common.mk` will be needed.*

### Render sample image
```text
$ ./ndt -f 1
```

## Defining Custom Scenes

A scene consists of a camera, a set of objects and a set of lights.
There are two ways to create a scene, the C API or a YAML file.

### C API

To create a custom scene using the C API, start by making a copy of `empty.c`
in the scenes directory.
```text
$ cd scenes
$ cp empty.c custom.c
```

Next, open `custom.c` (or whatever you called it) in a text editor or IDE.
There are three functions defined in this file.

---

The first function `scene_frames` is optional and returns the number of frames
to be rendered, if the scene describes multiple frames, (e.g., an animation).

**int scene_frames(int dimensions, char \*config);**
 
Parameters:
 * *dimensions* - The number of dimensions for the current render.
 * *config* - A custom configuration string passed via the `-u` flag.

Returns:
 * Number of frames to render.  This value can be overridden using the `-f`
 command-line option.

---

The second function `scene_setup` is required, and does all of the work of configuring a scene.
Near the top of `scene_setup` there must a call to `scene_init` which provides
a name for the scene as well as the number of dimensions that seen will
contain.
Change the name in the `scene_setup call from "empty" to "custom".
This will cause all images created by the custom scene to be placed in a newly
created "custom" subdirectory under the "images" directory.

**int scene_setup(scene \*scn, int dimensions, int frame, int frames, char \*config);**

The required function populates a scene structure with the camerea, objects
and lighting.

Parameters:
 * *scn* - The scene structure that will contain the scene.
 * *dimensions* - The number of dimensions for the current render.
 * *frame* - Current frame to be rendered (starting from zero).
 * *frames* - Total number of frames to be rendered.
 * *config* - A custom configuration string passed via the `-u` flag.

Returns:
 * Reserved

---

A third function `scene_cleanup` is optional, but provides means to cleanup
any memory or other persistent resources that a scene might use to maintain
state between frames.

**int scene_cleanup();**

The `scene_cleanup` function should free any additional persistent memory or
other resources that the scene setup function may have allocated to maintain
inter-frame state.
This function will only be called once, after all frames have been rendered.
If the scene doesn't allocate any persistent resources, this function can
be omitted.

Returns:
 * Reserved

#### Vectors

All positions and directions are represented using a n-dimensions vector structure `vectNd`.
Before a `vectNd` can be used, it first must be initialized with `vectNd_alloc` or `vectNd_calloc`
(e.g., `vectNd_alloc(&v, 5)` allocates a 5-dimensional vector). 
and when no longer needed, should always be freed with `vectNd_free`.

The easiest way to set a vector is the `vectNd_setStr` method.
`vectNd_setStr` takes a pointer to the vector and a string representation of
the vector (e.g., vectNd_setStr(&v, "1,2,3,4"), sets **v** to <1,2,3,4>).
Additionally `vectNd_set` can be used to modify individual elements of the
vector, (e.g., vectNd_set(&v, 0, 7) sets the first element of **v** to 7).

#### Camera

Each scene must have a camera that defines perspective from which the scene
will be viewed.

A camera must first be allocated and initialized:
```c
camera_alloc(&scn->cam, dimensions);
camera_reset(&scn->cam);
```

Once the camera is initialized, it can be places into the scene using the `camera_set_aim` function.
The `camera_set_aim` function is passed five parameters.
This first is a pointer to the camera to be aimed.
The second is a pointer to a vector from the origin to the point the scene
will be viewed from.
The third is a pointer to a vector from the origin to the point that will be
centered in the rendered image.
The fourth is an optional pointer to a vector (`NULL` if omitted), that gives
a vector that should point toward the top of the rendered image.
The fifth is a double that indicates a rotation (in radians) parallel to the
image plane to be applied before the camera is aimed.
In most cases, this should be zero.

```c
vectNd_setStr(&viewPoint,"60,0,0,0");
vectNd_setStr(&viewTarget,"0,0,0,0");
vectNd_set(&up_vect,1,10);  /* 0,10,0,0... */
camera_set_aim(&scn->cam, &viewPoint, &viewTarget, &up_vect, 0);
```

#### Lights

In order for anything to be visible in a scene, light sources are needed to illuminate the objects.
Each scene contains a set of light structures that provide illumination.

Make a call to `scene_alloc_light` to add a light to a scene.
This function takes as parameters a pointer to the scene as well as a pointer
to a light pointer (e.g., `scene_alloc_light(scn,&lgt)`, where lgt is of type
`light\*`).

Once a light has been added to the scene it needs to be configured.
There are secveral types of lights that each have different configuration requirements.

The simplest type of light is ambient light.
To configure an ambient light, set the type to `LIGHT_AMBIENT`, (e.g., `lgt->type = LIGHT_AMBIENT`).
Then to configure the color/intensity of the ambient light, set the `red`, `green`, and `blue` fields to a value between 0.0 and 1.0.

```c
light *lgt=NULL;
scene_alloc_light(scn,&lgt);
lgt->type = LIGHT_AMBIENT;
lgt->red = 0.5;
lgt->green = 0.5;
lgt->blue = 0.5;
```

Another, more complex light is a point light.
To configure a point light, set the type to `LIGHT_POINT`, (e.g., `lgt->type = LIGHT_POINT`).
Unlike ambient light, a point light has a position which is set by allocating and setting the `pos` field of the light structure.
Then the `red`, `green`, and `blue` fields give the intensity of the light in
each color channel.
Due to how light intensity falls off with distance, it is often necessary to
set the intensity of position based lighting to values in the hundreds, thousands, or more,
depending on the distance.

```c
light *lgt=NULL;
scene_alloc_light(scn,&lgt);
lgt->type = LIGHT_POINT;
vectNd_calloc(&lgt->pos,dimensions);
vectNd_setStr(&lgt->pos,"0,40,0,-40");
lgt->red = 300;
lgt->green = 300;
lgt->blue = 300;
```

Additional types of lighting are available, but these two should be enough to get started.

#### Objects

Many types of object primitives are available in ndt and are identified by a type string.
A new object is added to the scene by calling `scene_alloc_object`.
The parameters for `scene_alloc_object` are a pointer to the scene,cw
the number of dimensions,
a pointer to a pointer to an object,
and a string indicating the type of object being created.

Objects in ndt are defined as sets of positions, directions, sizes (doubles), flags
(integers), and other objects.

Once an object is allocated, elements are added to the appropriate sets using
a set of functions.
Each such function takes as its first parameter a pointer to the object, and
the value being added as a second parameter.
Positions are added with the `object_add_pos` function, directions with
`object_add_dir`, sizes with `object_add_size`, flags (integers) with
`object_add_flag`, and objects with `object_add_obj`.
When adding vectors or objects, a pointer is used, not the actual value.

For example, to add sphere to a 4-dimensional custom scene:
```c
object *obj = NULL;
scene_alloc_object(scn, 4, &obj, "sphere");
vectNd center;
vectNd_calloc(&center, 4);
vectNd_setStr(&center, "0,5,0,0");  /* centered at <1,2,3,4> */
object_add_pos(obj, &center);
object_add_size(obj, 5.0);  /* radius of 5 */
/* set color */
obj->red = 0.8;
obj->green = 0.1;
obj->blue = 0.1;
/* set reflectivity */
obj->red_r = 0.2;
obj->green_r = 0.2;
obj->blue_r = 0.2;
```
*Note: In general, the **dimensions** variable should be used whenever the number of dimensions is needed.*

Not all types of objects require elements in each set.
The number of elements required in each set can depend on the number of
dimensions.
Each object type provides a functions (`params`) that provides the number of
each type of element needed to fully describe an object.

#### Rendering Custom Scene

Once `custom.c` has been modified, it needs to be compiled into a shared object which can be loaded by ndt at runtime.
The make file in the scenes directory should do this automatically when `make`
is run in either the scenes directory or the top level ndt directory.

```text
$ cd ..
$ make
```

To actually render a scene, ndt needs to be told, using the `-s` options, to load the shared object
that contains the scene description.
The `-d` option indicates that the scene should be rendered using 4-dimensions.
```text
$ ./ndt -s scenes/custom.so -d 4
```

If the custom scene needs any additional configuration information passed into
it, the `-u` option can be used.

```text
$ ./ndt -s scenes/custom.so -u `configuration string`
```

### YAML

To create a custom scene using a YAML file, start by making a copy of
empty.yaml in the scenes directory.

```text
$ cd scenes
$ cp empty.yaml custom.yaml
```

Next, open custom.yaml (or whatever you called it) in a text editor.

It should look like this:
```yaml
---
scene: empty
dimensions: 4
camera:
  viewPoint: [60, 0, 0, 0]
  viewTarget: [0, 0, 0, 0]
  up: [0, 10, 0, 0]
lights:
- type: LIGHT_AMBIENT
  color: {red: 0.5, green: 0.5, blue: 0.5}
- type: LIGHT_POINT
  color: {red: 300, green: 300, blue: 300}
  pos: [0, 40, 0, -40]
objects:
- type: hplane
  dimensions: 4
  material:
    color: {red: 0.8, green: 0.8, blue: 0.8}
    reflectivity: {red: 0.5, green: 0.5, blue: 0.5}
  positions:
  - [0, -20, 0, 0]
  directions:
  - [0, 1, 0, 0]
```

The second and third line specify the name of the scene as well as the number
of dimensions in the scene.
Change the scene name on the second line from "empty" to "custom".
This will cause all images created by the custom scene to be placed in a newly created "custom" subdirectory under the "images" directory.

#### Vectors

All positions and directions are represented using a n-dimensions vectors.
In YAML an n-dimensional vector is a sequence of double values, (e.g., `[60, 0, 0, 0]` is the 4-dimensional vector <60,0,0,0>).

#### Camera

Each scene must have a camera that defines perspective from which the scene will be viewed.

A camera is described in a map named `camera`.
```yaml
camera:
  viewPoint: [60, 0, 0, 0]
  viewTarget: [0, 0, 0, 0]
  up: [0, 10, 0, 0]
```

Fields:
 * `viewPoint` - The point from which the scene will be viewed.
 * `viewTarget` - The point which should appear at at the center of the screen.
 * `up` - An optional vector which would point up in the final image.
 * `rotation` - An optional double which specified a rotation (in radians) applied to the camera.

#### Lights

In order for anything to be visible in a scene, light sources are needed to illuminate the objects.
Each scene contains a set of light structures that provide illumination.

Lights are described in a sequence named `lights` where each entry is a map
with multiple fields which depends on the type of the light.
```yaml
lights:
- type: LIGHT_AMBIENT
  color: {red: 0.5, green: 0.5, blue: 0.5}
- type: LIGHT_POINT
  color: {red: 300, green: 300, blue: 300}
  pos: [0, 40, 0, -40]
```

This describes two lights.
The first is ambient light that is applied evently to all objects.
The color/intensity of the ambient light, set the red, green, and blue fields to a value between 0.0 and 1.0.
The second is a point light source, which has a position and can cast shadows.
Due to how light intensity falls off with distance, it is often necessary to set the intensity of position based lighting to values in the hundreds, thousands, or more, depending on the distance.

#### Objects

Many types of object primitives are available in ndt and are identified by a type string.

Objects are described in a sequence named `objects`, where each entry is a map
with multiple fields which depends on the type of the object.

In the "empty" scene there is actually one object.
```yaml
- type: hplane
  dimensions: 4
  material:
    color: {red: 0.8, green: 0.8, blue: 0.8}
    reflectivity: {red: 0.5, green: 0.5, blue: 0.5}
  positions:
  - [0, -20, 0, 0]
  directions:
  - [0, 1, 0, 0]
```

Fields:
 * `type` - Specifies the type of object.
 * `dimensions` - Number of dimensions for objects (should always match the dimensions in the scene).
 * `materal` - A map that provides color and other visual properties of the object.
 * `positions` - A sequence of vectors that give the location of the object (e.g. center of a sphere).
 * `directions` - A sequence of vectors that give non-location base directions (e.g., normal vectors for a surface).
 * `sizes` - A sequence of doubles that give magnitudes the objects might need (e.g., the radius of a sphere).
 * `flags` - A sequence of integers that provide any discrete values the objects needs.

To add a sphere to the custom scene, insert the following snippet into the
objects section:
```yaml
- type: sphere
  dimensions: 4
  material:
    color: {red: 0.8, green: 0.1, blue: 0.1}
    reflectivity: {red: 0.2, green: 0.2, blue: 0.2}
  positions:
  - [0, 5, 0, 0]
  sizes:
  - 5.0
```

#### Rendering Custom Scene

To render the custom scene, a special yaml scene, specified with `-s` options, is used and the yaml file is
passed as a custom parameter, passed with the `-u` option.
The `-d` option indicates that the scene should be rendered using 4-dimensions.
```text
$ ./ndt -s scenes/yaml.so -u scenes/custom.yaml -d 4
```
