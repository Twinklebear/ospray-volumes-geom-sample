# OSPRay Volume + Spheres & Streamlines Sample

This sample shows how to setup a scene with a volume, some spheres and
some streamlines using OSPRay. It fills a volume with a gradient and
places randomly positioned spheres and streamlines within it.


## Building

You'll need OSPRay, Embree, and TBB. Then you can build with CMake:

```bash
cmake -Dembree_DIR=<path to embree> \
      -Dospray_DIR=<path to ospray>/lib/cmake/ospray-<version>/ \
      -DTBB_ROOT=<path to TBB>
```

See the [OSPRay Documentation for more](http://www.ospray.org/documentation.html).

You'll see something like the image below saved to `sample_render.png` when you run the app.

![sample img](https://i.imgur.com/OdqKcK2.png)


