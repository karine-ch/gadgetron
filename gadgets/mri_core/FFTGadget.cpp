#include "FFTGadget.h"
#include "hoNDFFT.h"

namespace Gadgetron{

FFTGadget::FFTGadget()
  : image_counter_(0)
{

}


int FFTGadget::process( GadgetContainerMessage<IsmrmrdReconData>* m1)
{
    
    //Iterate over all the recon bits
    for(std::vector<IsmrmrdReconBit>::iterator it = m1->getObjectPtr()->rbit_.begin();
        it != m1->getObjectPtr()->rbit_.end(); ++it)
    {
        //Grab a reference to the buffer containing the imaging data
        IsmrmrdDataBuffered & dbuff = it->data_;

        //7D, fixed order [RO, E1, E2, CHA, LOC, N, S]
        uint16_t RO = dbuff.data_.get_size(0);
        uint16_t E1 = dbuff.data_.get_size(1);
        uint16_t E2 = dbuff.data_.get_size(2);
        uint16_t CHA = dbuff.data_.get_size(3);
        uint16_t LOC = dbuff.data_.get_size(4);
        uint16_t N = dbuff.data_.get_size(5);
        uint16_t S = dbuff.data_.get_size(6);

        //Each image will be [RO,E1,E2,CHA] big
        std::vector<size_t> img_dims(4);
        img_dims[0] = RO;
        img_dims[1] = E1;
        img_dims[2] = E2;
        img_dims[3] = CHA;

        //Loop over S and N and LOC
        for (uint16_t s=0; s < S; s++) {                
            for (uint16_t n=0; n < N; n++) {
                for (uint16_t loc=0; loc < LOC; loc++) {
                    
                    //Create a new image
                    GadgetContainerMessage<ISMRMRD::ImageHeader>* cm1 = 
                            new GadgetContainerMessage<ISMRMRD::ImageHeader>();
                    GadgetContainerMessage< hoNDArray< std::complex<float> > >* cm2 = 
                            new GadgetContainerMessage<hoNDArray< std::complex<float> > >();
                    cm1->cont(cm2);
                    //TODO do we want an image attribute string?  
                    try{cm2->getObjectPtr()->create(&img_dims);}
                    catch (std::runtime_error &err){
                        GEXCEPTION(err,"Unable to allocate new image array\n");
                        cm1->release();
                        return GADGET_FAIL;
                    }

                    //Set some information into the image header
                    //Use the middle header for some info
                    //[E1, E2, LOC, N, S]
                    ISMRMRD::AcquisitionHeader & acqhdr = dbuff.headers_(dbuff.sampling_.sampling_limits_[1].center_,
                                                                         dbuff.sampling_.sampling_limits_[2].center_,
                                                                         loc, n, s);
                    
                    cm1->getObjectPtr()->matrix_size[0]     = RO;
                    cm1->getObjectPtr()->matrix_size[1]     = E1;
                    cm1->getObjectPtr()->matrix_size[2]     = E2;
                    cm1->getObjectPtr()->field_of_view[0]   = dbuff.sampling_.recon_FOV_[0];
                    cm1->getObjectPtr()->field_of_view[1]   = dbuff.sampling_.recon_FOV_[1];
                    cm1->getObjectPtr()->field_of_view[2]   = dbuff.sampling_.recon_FOV_[2];
                    cm1->getObjectPtr()->channels           = CHA;
                    cm1->getObjectPtr()->slice   = acqhdr.idx.slice;

                    memcpy(cm1->getObjectPtr()->position, acqhdr.position, sizeof(float)*3);
                    memcpy(cm1->getObjectPtr()->read_dir, acqhdr.read_dir, sizeof(float)*3);
                    memcpy(cm1->getObjectPtr()->phase_dir, acqhdr.phase_dir, sizeof(float)*3);
                    memcpy(cm1->getObjectPtr()->slice_dir, acqhdr.slice_dir, sizeof(float)*3);
                    memcpy(cm1->getObjectPtr()->patient_table_position, acqhdr.patient_table_position, sizeof(float)*3);
                    cm1->getObjectPtr()->data_type = ISMRMRD::ISMRMRD_CXFLOAT;
                    cm1->getObjectPtr()->image_index = ++image_counter_;

                    //Copy the 4D data block [RO,E1,E2,CHA] for this loc, n, and s into the output image
                    memcpy(cm2->getObjectPtr()->get_data_ptr(), &dbuff.data_(0,0,0,0,loc,n,s), RO*E1*E2*CHA*sizeof(std::complex<float>));

                    //Do the FFTs in place
                    hoNDFFT<float>::instance()->ifft(cm2->getObjectPtr(),0);
                    hoNDFFT<float>::instance()->ifft(cm2->getObjectPtr(),1);
                    if (E2>1) {
                        hoNDFFT<float>::instance()->ifft(cm2->getObjectPtr(),2);
                    }

                    //Pass the image down the chain
                    if (this->next()->putq(cm1) < 0) {
                        return GADGET_FAIL;
                    }
                }
            }
        }
    }
    return GADGET_OK;  

}

GADGET_FACTORY_DECLARE(FFTGadget)
}
