#include <iostream>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebufalgo.h>
#include <stdint.h>

void image_read(char const *bokeh_kernel_filename, char const *bokeh_kernel_filename_out){

    std::cerr << "READING IMAGE WITH OIIO" << std::endl;
    OpenImageIO::ImageInput *in = OpenImageIO::ImageInput::open (bokeh_kernel_filename);
    if (! in){
        std::cerr << "FAILED TO READ IMAGE" << std::endl;
        return nullptr;
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


    float *buffer = spec.data();
    OpenImageIO::ImageBuf buf(spec, buffer);
    OpenImageIO::ImageBuf out;
    OpenImageIO::ImageBufAlgo::flip(out, buf);
    out.write(std::string(bokeh_kernel_filename_out));


}



int main(){
    //image_read("vertical.png");
    image_read("imgs/vertical.png", "imgs/vertical_flipped.png");
}

