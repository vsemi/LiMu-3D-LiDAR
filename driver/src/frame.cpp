#include "frame.hpp"

#include <stdint.h>
#include <iostream>

Frame::Frame(uint16_t dataType_, uint64_t frame_id_, uint16_t width_, uint16_t height_, uint16_t payloadOffset) :
frame_id(frame_id_),
dataType(dataType_),
width(width_),
height(height_),
px_size(sizeof(uint16_t)),
distData(std::vector<uint8_t>(width * height * px_size)), //16 bit
amplData(std::vector<uint8_t>(width * height * px_size)), //16 bit
dcsData(std::vector<uint8_t> (width * height * px_size * 4)), //16 bit 4 dcs
payloadHeaderOffset(payloadOffset)
{    
	n_points            = width * height;
	data_depth          = new float[n_points];
	data_3d_xyz_rgb     = new float[n_points * 8];
	data_2d_bgr         = new uint8_t[n_points * 3];
	saturated_mask      = new uint8_t[n_points];
	data_grayscale      = new uint8_t[n_points];
	data_amplitude      = new float[n_points];
}

Frame::~Frame()
{
	delete[] data_depth;
	delete[] data_3d_xyz_rgb;
	delete[] data_2d_bgr;
	delete[] saturated_mask;
	delete[] data_grayscale;
	delete[] data_amplitude;
}

void Frame::sortData(const Packet &data)
{    
    int i,j;

    if(dataType == Frame::AMPLITUDE){ //distance - amplitude

        int sz = payloadHeaderOffset + width * height * px_size * 2;
        for(j=0, i = payloadHeaderOffset; i < sz; i+=4, j+=2){
            if(data[i+1] > 61) {
                //std::cout << "   ===> data[i+1]: " << data[i+1] << std::endl;
                 //continue; 
            }
            distData[j]   = data[i];
            distData[j+1] = data[i+1];
            amplData[j]   = data[i+2];
            amplData[j+1] = data[i+3] & 0x0f;
        }

    }else if(dataType == Frame::DISTANCE){ //distance

        int sz = payloadHeaderOffset + width * height * px_size;
        for(j=0, i = payloadHeaderOffset; i < sz; i+=2, j+=2){
            if(data[i+1] > 61) {
                //std::cout << "   ===> data[i+1]: " << data[i+1] << std::endl;
                 //continue; 
            }
            distData[j]    = data[i];
            distData[j+1]  = data[i+1];

        }

    }else if(dataType == Frame::GRAYSCALE){ //grayscale

        int sz = payloadHeaderOffset + width * height * px_size;
        for(j=0, i = payloadHeaderOffset; i < sz; i+=2, j+=2){
            if(data[i+1] > 61) {
                //std::cout << "   ===> data[i+1]: " << data[i+1] << std::endl;
                 //continue; 
            }
            amplData[j]    = data[i];
            amplData[j+1]  = data[i+1] & 0x0f;
        }

    }else{ //DCS

        int sz = payloadHeaderOffset + width * height * px_size * 4;
        for(j=0, i = payloadHeaderOffset; i < sz; i+=2, j+=2){
            if(data[i+1] > 61) {
                //std::cout << "   ===> data[i+1]: " << data[i+1] << std::endl;
                 //continue; 
            }
            dcsData[j]    = data[i];
            dcsData[j+1]  = data[i+1] & 0x0f;
        }
    }

    //temperature = (data[22] << 8) + data[21];
    temperature = (data[21] << 8) + data[22];
}
