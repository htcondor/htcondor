package main

import (
    "fmt"
    producer "condor_producer_mytoken/mytokenproducer"
)

func main() {

    //-- general settings
    tokendata := new(producer.TokenData)
    producer.Configure(tokendata)
    producer.Get_encryption_key(tokendata)

    //-- Produce or renew credentials if needed
    if producer.Renew(tokendata) {

        //-- user credential directory
        producer.Create_credential_dir(tokendata)

        //-- mytoken generation
        producer.Create_mytoken(tokendata)
        producer.Encrypt_mytoken(tokendata)
        producer.Write_token(tokendata,"top")

        //-- access token generation
        producer.Create_access_token(tokendata)
        producer.Write_token(tokendata,"use")
	producer.Lifetime(tokendata)
	fmt.Printf("Your credential has been successfully created! \n\n")
	fmt.Printf("Its remaining life time is %s.\n\n",tokendata.Mytoken_time_dhs)
    }
}
