

    /**
     * AviaryLocatorServiceSkeleton.h
     *
     * This file was auto-generated from WSDL for "AviaryLocatorService|http://grid.redhat.com/aviary-locator/" service
     * by the WSO2 WSF/CPP Version: 1.0
     * AviaryLocatorServiceSkeleton WSO2 WSF/CPP Skeleton for the Service Header File
     */
#ifndef AVIARYLOCATORSERVICESKELETON_H
#define AVIARYLOCATORSERVICESKELETON_H

    #include <OMElement.h>
    #include <MessageContext.h>
   
     #include <AviaryLocator_Locate.h>
    
     #include <AviaryLocator_LocateResponse.h>
    
namespace AviaryLocator{
    

   /** we have to reserve some error codes for adb and for custom messages */
    #define AVIARYLOCATORSERVICESKELETON_ERROR_CODES_START (AXIS2_ERROR_LAST + 2500)

    typedef enum
    {
        AVIARYLOCATORSERVICESKELETON_ERROR_NONE = AVIARYLOCATORSERVICESKELETON_ERROR_CODES_START,

        AVIARYLOCATORSERVICESKELETON_ERROR_LAST
    } AviaryLocatorServiceSkeleton_error_codes;

    


class AviaryLocatorServiceSkeleton
{
        public:
            AviaryLocatorServiceSkeleton(){}


     




		 


        /**
         * Auto generated method declaration
         * for "locate|http://grid.redhat.com/aviary-locator/" operation.
         * 
         * @param _locate of the AviaryLocator::Locate
         *
         * @return AviaryLocator::LocateResponse*
         */
        

         virtual 
        AviaryLocator::LocateResponse* locate(wso2wsf::MessageContext *outCtx ,AviaryLocator::Locate* _locate);


     



};


}



        
#endif // AVIARYLOCATORSERVICESKELETON_H
    

