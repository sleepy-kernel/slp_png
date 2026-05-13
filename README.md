# incomplete README

# Provide:
- png_imread
- png_imsave
- image_format
- image_unformat
- image_convert_to_8bit
- image_convert_to_16bit
- image_crop
- image_linear_transform
- image_convert_G8_to_RGBA32
- image_convert_G8_to_RGBA32
- image_convert_GA16_to_RGBA32
- image_convert_G16_to_RGBA64
- image_convert_GA32_to_RGBA64
- image_copy


# Description:

**struct Image**
- contains information about the image ( height, width, channel, bit depth ), the buffer to the data and the return code called state
- returning with state is kinda unneccessary though, I'm quite regret about that. I should've just set the buffer = NULL

**png_imread**
- read and decode from a .png file into ram
- did not format anything yet, all this function do is read, uncompress, defilter and return
- for corlor type 0/2/4/6 this will read IHDR, IDAT, IEND chunks
- for color type 3 this will read IHDR, PLTE, tRNS, IDAT, IEND chunks
- example:

Image a = png_imread("/path/to/your/iamge");

if (a.state != 0) printf("png_imread: error);

else printf("png_imread: success");

- does not support Interlace method 1 ( Adam 7 )
- support bit depth: 1/2/4/8/16
- support color type: 0/2/3/4/6 ( notice that color type 3 will be convert into RGBA )


**png_imsave**
- save the image from ram to a file
- save from whatever imread read
- imsave only filter, compress, then save the image, it does not modify or do any extra formatting to save the iamge
- imsave only write IHDR, IDAT, IEND chunks
- example:

if (png_imsave(a, "/path/to/your/image") != 0) printf("png_imsave: error");

else printf("png_imsave: success");

- does not support Interlace method 1 ( Adam 7 )
- support bit depth: 1/2/4/8/16
- support color type: 0/2/4/6


**image_format**
- for bit depth 1/2/4 this will unpack such that each element align to 8 bit
- for bit depth 8 this do nothing
- for bit depth 16 this will swap edian, so this ONLY WORKS ON LITTLE EDIAN MACHINE, for big edian machine this is WRONG
- I don't really care about that unless someone really need this to work on a big edian machine
- notice that you'll have to unformat before imsave
- this function exist in order the remove the dupplicate work in many image_* functions from imtransform.h which can save more performance


**image_unformat**
- the reverse of image_format
- packing 1/2/4 bit depth image back
- the typical work flow is:

- call png_imread
- call image_format
- do multiple transform
- call image_unformat
- call png_imsave

- this will be more light weight and explicit since if transform functions want to support bit depth 1/2/4 they'll have to call image_format anyway
- NOTICE that both image_format and image_unformat assume to work on little edian so it might break on big edian machine

  


