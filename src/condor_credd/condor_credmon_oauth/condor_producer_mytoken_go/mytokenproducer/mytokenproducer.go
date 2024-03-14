package mytokenproducer

import (
    "fmt"
    "os"
    "time"
    "strings"
    "bufio"
    "io"
    "github.com/golang-jwt/jwt"
    "github.com/nogproject/nog/backend/pkg/pwd"
    mytokenlib "github.com/oidc-mytoken/lib" 
    api "github.com/oidc-mytoken/api/v0"
    fernet "github.com/fernet/fernet-go"
    "path/filepath"
    "strconv"
    "os/user"
    "syscall"
) 

type TokenData struct {
    Encryption_key, Encryption_key_file string

    Oauth_issuer_url, Oauth_issuer_name string
    Mytoken_issuer_url, Mytoken_profile string
    Cred_dir, Cred_dir_user string

    Mytoken, Mytoken_encrypted, Mytoken_file string
    Mytoken_server *mytokenlib.MytokenServer

    Access_token, Access_token_file string
}

func Check(err error) {
    if err != nil {
        fmt.Printf("error: %s\n", err.Error())
        os.Exit(1)
    }
}

func PrintDebug(format string, a ...any) {
    log_level := Parameter("PRODUCER_OAUTH_LOG_LEVEL")
    if log_level == "DEBUG" {
        fmt.Printf(format, a...)
    }
 }

func Parameter(parameter string) string {
     var condor_config string = "/etc/condor/config.d/"
     var parameter_value string = "undefined"
     FindParameter(condor_config, parameter, &parameter_value)
     return parameter_value
}

func FindParameter(path_directory string, parameter_required string, parameter_value *string) {

    filepath.Walk(path_directory, func(filename string, info os.FileInfo, err error) error {        

        if err != nil || len(filename) == 0 {
            return err
        }

        file, err := os.Open(filename)    
        Check(err)

        defer file.Close()

        if !info.IsDir() && !strings.Contains(filename,"~") {
        
            reader := bufio.NewReader(file)

            for {

                line, err := reader.ReadString('\n')

                if equal := strings.Index(line, "="); equal >= 0 { 
                    parameter := strings.TrimSpace(line[:equal-1])

                    if len(line) > equal {
                        if parameter == parameter_required {
                            *parameter_value = strings.TrimSpace(line[equal+1:])
                            break
                        }
                    }       
                } 

                if err == io.EOF {
                    break
                }

                Check(err)
            } 
        }

        return nil
    })
   
    if *parameter_value == "undefined" {
        fmt.Printf("Parameter %s not found! \n",parameter_required)
        fmt.Printf("Please define the parameter %s! \n", parameter_required)
        os.Exit(1)
    }
}

func Configure(tokendata *TokenData) {

    fmt.Printf("\n\n")
    fmt.Printf("Hello %s! You are going to submit your HTCondor jobs. \n\n", pwd.Getpwuid(uint32(os.Getuid())).Name)

    tokendata.Encryption_key = "undefined"
    tokendata.Encryption_key_file = Parameter("SEC_ENCRYPTION_KEY_DIRECTORY")

    tokendata.Oauth_issuer_url = Parameter("OAUTH_ISSUER_URL")
    tokendata.Oauth_issuer_name = Parameter("OAUTH_ISSUER_NAME")

    tokendata.Mytoken_issuer_url = Parameter("MYTOKEN_ISSUER_URL")
    tokendata.Mytoken_profile = Parameter("MYTOKEN_PROFILE")

    tokendata.Cred_dir = Parameter("SEC_CREDENTIAL_DIRECTORY_OAUTH")
    tokendata.Cred_dir_user = tokendata.Cred_dir + "/" + pwd.Getpwuid(uint32(os.Getuid())).Name

    tokendata.Mytoken = "undefined"
    tokendata.Mytoken_encrypted = "undefined"
    tokendata.Mytoken_file = tokendata.Cred_dir_user + "/" + tokendata.Oauth_issuer_name + ".top"

    server, err := mytokenlib.NewMytokenServer(tokendata.Mytoken_issuer_url)
    Check(err)
    tokendata.Mytoken_server = server

    tokendata.Access_token = "undefined"
    tokendata.Access_token_file = tokendata.Cred_dir_user + "/" + tokendata.Oauth_issuer_name + ".use"

    PrintDebug("Configuration successfully retrieved: \n\n")
    PrintDebug("OAUTH ISSUER URL: %s \n", tokendata.Oauth_issuer_url)
    PrintDebug("OAUTH ISSUER NAME: %s \n\n", tokendata.Oauth_issuer_name)
    PrintDebug("MYTOKEN ISSUER URL: %s \n", tokendata.Mytoken_issuer_url)
    PrintDebug("MYTOKEN PROFILE: %s \n\n", tokendata.Mytoken_profile)
    PrintDebug("CREDENTIAL DIRECTORY: %s \n", tokendata.Cred_dir)
    PrintDebug("USER CREDENTIAL DIRECTORY: %s \n\n", tokendata.Cred_dir_user)
    PrintDebug("MYTOKEN CREDENTIAL FILE: %s \n", tokendata.Mytoken_file)
    PrintDebug("ACCESS TOKEN CREDENTIAL FILE: %s \n\n", tokendata.Access_token_file)
}

func Get_encryption_key(tokendata *TokenData) { 
    key, err := os.ReadFile(tokendata.Encryption_key_file)
    tokendata.Encryption_key = string(key)
    Check(err)
    PrintDebug("Encryption key for Fernet algorithm successfully retrieved \n\n")
}

func Create_credential_dir(tokendata *TokenData) {
    if _, err := os.Stat(tokendata.Cred_dir_user); os.IsNotExist(err) {
        if err := os.Mkdir(tokendata.Cred_dir_user, os.FileMode(0770)); err == nil {            
            PrintDebug("Credential directory successfully created for user %s: %s\n\n", pwd.Getpwuid(uint32(os.Getuid())).Name, tokendata.Cred_dir_user)
        } 
    } else {
        PrintDebug("Credential directory for user %s already exists: %s\n\n", pwd.Getpwuid(uint32(os.Getuid())).Name, tokendata.Cred_dir_user)
    }   

    info, _ := os.Stat(tokendata.Cred_dir_user)
    stat := info.Sys().(*syscall.Stat_t)

    uid := stat.Uid
    gid := stat.Gid

    uid_string := strconv.FormatUint(uint64(uid), 10)
    gid_string := strconv.FormatUint(uint64(gid), 10)

    user_info, _ := user.LookupId(uid_string)
    group_info, _ := user.LookupGroupId(gid_string)

    PrintDebug("Directory %s belongs to user: %s \n\n", tokendata.Cred_dir_user, user_info.Username)
    PrintDebug("Directory %s belongs to group: %s \n\n", tokendata.Cred_dir_user, group_info.Name)
}

func Create_token_file(filename string) {

    var token_type string
    if strings.Contains(filename, "top") {
        token_type = "Mytoken credential"
    } else if strings.Contains(filename, "use") {
        token_type = "Access token credential"
    } else {
        fmt.Printf("File type not recognized: %s\n", filename)
        os.Exit(1)
    }

    if _, err := os.Stat(filename); os.IsNotExist(err) {
        file, _ := os.Create(filename)
        _ = os.Chmod(filename,0600)
        PrintDebug("%s file successfully created: %s \n\n", token_type, filename)
        defer file.Close()
    } else {
        PrintDebug("%s file already exists: %s \n\n", token_type, filename)
        Check(err)
    }	
}

func Create_mytoken(tokendata *TokenData) {

    info, err := os.Stat(tokendata.Mytoken_file)
    Check(err)

    if info.Size() == 0 {
        Mytoken_request := api.GeneralMytokenRequest{
            Issuer: tokendata.Oauth_issuer_url, 
            ApplicationName: "htcondor",
            Name: "htcondor",
            IncludedProfiles: api.IncludedProfiles{tokendata.Mytoken_profile},
        }

        callbacks := mytokenlib.PollingCallbacks{

            Init: func(authorizationURL string) error {
                fmt.Printf("Please visit the following url in order to generate your credentials: %s \n\n", authorizationURL)
                return nil
            },

            Callback: func(interval int64, iteration int) {
                if iteration == 0 {
                    fmt.Printf("Starting polling ...")
                    return
                }
                if int64(iteration)%(15/interval) == 0 {
                    fmt.Printf(".")
                }
            },

            End: func() {
                fmt.Printf("\n\n")
                PrintDebug("Mytoken credential successfully created \n\n")
            },
        }

        Mytoken_endpoint := tokendata.Mytoken_server.Mytoken
        Mytoken_response, err := Mytoken_endpoint.APIFromAuthorizationFlowReq(Mytoken_request, callbacks)
        Check(err)
  
        tokendata.Mytoken = Mytoken_response.Mytoken    
    } else {
        PrintDebug("Mytoken credential already exists \n\n")
    }
}

func Encrypt_mytoken(tokendata *TokenData) {
    info, err := os.Stat(tokendata.Mytoken_file)
    Check(err)

    if info.Size() == 0 {
        key := fernet.MustDecodeKeys(tokendata.Encryption_key)
        if encrypted, err := fernet.EncryptAndSign([]byte(tokendata.Mytoken), key[0]); err == nil {
            tokendata.Mytoken_encrypted = string(encrypted)
            PrintDebug("Mytoken credential successfully encrypted \n\n")
        } else {
            Check(err)
        }
    } else {
        PrintDebug("Mytoken credential already encrypted \n\n")
    }
}

func Decrypt_mytoken(tokendata *TokenData) string {
    Mytoken_encrypted, err := os.ReadFile(tokendata.Mytoken_file)
    Check(err)

    key := fernet.MustDecodeKeys(tokendata.Encryption_key)
    Mytoken_decrypted := fernet.VerifyAndDecrypt([]byte(Mytoken_encrypted), 60*time.Second, key)
    return(string(Mytoken_decrypted))
}


func Write_token(filename string, token string) {

    var token_type string
    if strings.Contains(filename, "top") {
        token_type = "Encrypted Mytoken credential"
    } else if strings.Contains(filename, "use") {
        token_type = "Access token credential"
    } else {
        fmt.Printf("File type not recognized: %s\n", filename)
        os.Exit(1)
    }

    info, err := os.Stat(filename)
    Check(err)

    if info.Size() == 0 {
        file, err := os.OpenFile(filename, os.O_WRONLY, 0644)
        Check(err)
        if _, err := fmt.Fprintln(file, token); err == nil {
            PrintDebug("%s successfully written to file: %s \n\n", token_type, filename)
        } else {
            Check(err)
        }
    } else {
        PrintDebug("%s already written to file: %s \n\n", token_type, filename)
    }

    if strings.Contains(filename, "use") {
        creation_time := info.ModTime().Unix()
        time_now := time.Now().Unix()
        elapsed_time := time_now - creation_time

        token_data, err_data := os.ReadFile(filename)
        Check(err_data)

        token_data_trimmed := strings.TrimSpace(string(token_data))

        claims := jwt.MapClaims{}
        var parser jwt.Parser
        _, _, err := parser.ParseUnverified(string(token_data_trimmed), claims)
        Check(err)

        access_token_lifetime := claims["exp"].(float64) - claims["iat"].(float64)
        access_token_time := access_token_lifetime - float64(elapsed_time)

        PrintDebug("Access token credential life time: %d seconds \n\n", int(access_token_lifetime))
        PrintDebug("Access token credential remaining life time: %d seconds \n\n", int(access_token_time))
    }
}

func Create_access_token(tokendata *TokenData) {

    info, err := os.Stat(tokendata.Access_token_file)
    Check(err)

    if info.Size() == 0 {
        access_token_response := Get_access_token_response(tokendata)
        tokendata.Access_token = access_token_response.AccessToken
        PrintDebug("Access token credential successfully created \n\n")
    } else {
        PrintDebug("Access token credential already exists \n\n")
    }
}

func Get_access_token_response(tokendata *TokenData) api.AccessTokenResponse {
    var scopes, audiences []string
    var comment string

    Mytoken_decrypted := Decrypt_mytoken(tokendata)

    access_token_endpoint := tokendata.Mytoken_server.AccessToken
    access_token_response, err := access_token_endpoint.APIGet(Mytoken_decrypted, "" , scopes, audiences, comment)

    Check(err)

    return(access_token_response)
}