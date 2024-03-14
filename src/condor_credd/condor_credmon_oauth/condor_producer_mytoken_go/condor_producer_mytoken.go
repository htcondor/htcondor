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

    producer.Create_credential_dir(tokendata)    

    //-- mytoken generation 

    producer.Create_token_file(tokendata.Mytoken_file) 

    producer.Create_mytoken(tokendata)

    producer.Encrypt_mytoken(tokendata)

    producer.Write_token(tokendata.Mytoken_file, tokendata.Mytoken_encrypted)

    //-- access token generation

    producer.Create_token_file(tokendata.Access_token_file)

    producer.Create_access_token(tokendata)

    producer.Write_token(tokendata.Access_token_file, tokendata.Access_token)

    fmt.Printf("Your HTCondor jobs will now be submitted! \n")
}
