#include <iostream>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebufalgo.h>
#include <stdint.h>

void image_read(char const *bokeh_kernel_filename){

    std::cerr << "READING IMAGE WITH OIIO" << std::endl;
    OpenImageIO::ImageInput *in = OpenImageIO::ImageInput::open (bokeh_kernel_filename);
    if (! in){
        return;
    }
    const OpenImageIO::ImageSpec &spec = in->spec();
    int xres = spec.width;
    int yres = spec.height;
    int channels = spec.nchannels;
    std::vector<uint8_t> pixels (xres*yres*channels);
    in->read_image (OpenImageIO::TypeDesc::UINT8, &pixels[0]);
    in->close ();
    delete in;

    std::cerr << "IMAGEWIDTH: " << xres << std::endl;
    std::cerr << "IMAGEHEIGHT: " << yres << std::endl;
    std::cerr << "IMAGECHANNELS: " << channels << std::endl;


    for (int i = 0; i < xres*yres*channels; i++)
    {
        std::cout << (int)pixels[i] << std::endl;
    }


    // Resize the image to xres-1, yres-1, using the default filter
    OpenImageIO::ImageBuf Src (bokeh_kernel_filename);
    OpenImageIO::ImageBuf Dst;
    int newXRes = xres - 1;
    int newYRes = yres - 1;
    OpenImageIO::ROI roi (0, newXRes, 0, newYRes, 0, 1, 0, Src.nchannels());
    OpenImageIO::ImageBufAlgo::resize (Dst, Src, "", 0, roi);


}

bool resizeImage(const char* inFilename, const char* outFilename, int width)
{
    OpenImageIO::ImageBuf Src (inFilename);
    bool ok = Src.read();
    if (ok != true){
        std::cerr << "resizeImage: Could not read image!" << std::endl;
        return false;
    }
    std::cout << "read image" << std::endl;
    OpenImageIO::ImageBuf Dst;
    OpenImageIO::ROI roi (0, width, 0, width, 0, 1, /*chans:*/ 0, Src.nchannels());
    ok = OpenImageIO::ImageBufAlgo::resize (Dst, Src, "", 0, roi, 1);
    if (ok != true){
        std::cerr << "resizeImage: Could not resize image!" << std::endl;
        return false;
    }
    std::cout << "resized image" << std::endl;
    ok = Dst.write(outFilename);

    if (ok != true) {
        std::cerr << "resizeImage: Could not write image." << std::endl;
        return false;
    }
    return true;
}

int main(){
    //image_read("vertical.png");
    resizeImage("vertical.png", "vertical_resized.png", 50);
}




