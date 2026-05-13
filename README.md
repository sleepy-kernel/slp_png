## This meant to be an open source lightweight, modern, fast PNG decoder/encoder

- AVX2, SSE2 support
- Still depends on zlib
- Compatibility is meant to be high such that it can run on a microcontroler (still in progress)
    - As I cannot test with all hardware so I'd need help in testing
- Performance is already high.
    - In current version deflate/inflate runtime is more than 80% of the total runtime even at compression level 1, which means the runtime is just zlib runtime


## Contribute

- I appreciate all your help
- I will definitely respond to your pull request
- Just freely share your idea you don't have to be formal


## Basic usage
```C
#include <slp_png.h>
#include <stdio.h>

int main()
{
    slp_image your_image = slp_png_read("/path/to/your/image");
    if (your_image.buffer == NULL) return 1;

    int ret = slp_png_write(your_image, "/path/to/where/to/write");
    if (ret != 0) return 1;

    return 0;
}
```
