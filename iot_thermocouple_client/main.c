/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.c
 * Author: naima
 *
 * Created on July 24, 2023, 6:10 PM
 * Github https://github.com/naimaryan1/iot_thermocouple_client.git
 * 
 * prerequisite (needs a network connection): 
 * 1) sudo touch /var/log/heater
 * 2) sudo chmod 666 /var/log/heater
 * 3) sudo touch /var/log/status
 * 4) sudo chmod 666  /var/log/status
 * 5) Requires json-c libs:http://json-c.github.io/json-c/json-c-current-release/doc/html/index.html
 * 6) Requires  libjson-c.so file
 * sudo apt-get update
   sudo apt-get install libjson-c-dev
   sudo ldconfig
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <float.h>
#include <stdbool.h>
#include <json-c/json.h>
#include <json-c/json_object.h>
#include <curl/curl.h>
#include <pthread.h>
#include <unistd.h>
#define MAX_INPUT_LENGTH_1 4
#define MAX_INPUT_LENGTH_2 10
/*
 * Global Variables 
 */
const char* TEMP_PATH = "/tmp/temp";
const char* HEATER_STATUS_PATH = "/var/log/heater";
const char* LOG_MESSAGES_PATH = "/var/log/messages";
const char* STATUS_PATH = "/var/log/status";
const char* HEATER_ON = "1";
const char* HEATER_OFF = "0";
const char* NO_ACTION_ON = "no action, already ON=1\n";
const char* NO_ACTION_OFF = "no action, already ON=1\n";
volatile int CONTINUE = 1;
char* heat_status = "off";
//init set points
float set_point_1_temp = 0;
float set_point_2_temp = 0;
float set_point_3_temp = 0;
//init set point times
char* set_point_time_1_hh_mm = "HH:MM\0";
char* set_point_time_2_hh_mm = "HH:MM\0";
char* set_point_time_3_hh_mm = "HH:MM\0";
//struct to store config_json values that passed in via cmd 

typedef struct WebServiceConfig {
    char web_service_base_url[1024];
    char web_service_base_port[6];
    char create_config[64];
    char read_config[64];
    char update_config[64];
    char delete_config[64];
    char iot_id[16];
} WebServiceConfig;
WebServiceConfig webServiceConfig;

/*
 * Function Declarations - start
 */
void print_configs();
/*read the current temp from the file*/
float read_current_temp(const char* path_to_temp);
/* Turn off by writing "1" /var/log/heater.  <on|off> : <timestamp>*/
/* action:= <on|off> timstamp:=<posix time of action>*/
void heat_toggle(const char* path, const char* status);
/*utc time*/
void get_utc_time(char *timestamp, size_t timestamp_size);
void log_message(const char* path_to_log, const char* msg);
/*The configuration file shall configure Service endpoint (e.g. http://<some_host>:8000)*/
void set_service_endpoint(char* service_endpoint);
/*log messages to log files*/
void log_program_data(char* message);
void set_temp_and_time(float temp, char* time_hour_min);
/*communicate*/
void send_status_http(float temp, char* status, char* state);
/*process*/
int process(int argc, char** argv);
/*isHeaterOn*/
int isHeaterOn(const char* path);
/*read JSON from path*/
/*References:http://json-c.github.io/json-c/json-c-current-release/doc/html/index.html*/
bool readJSONConfig(const char *filepath);
/*create json msg*/
char* json_http_message(char* id, char* state, char* temp,
        float set_point_temp_1,
        float set_point_temp_2,
        float set_point_temp_3,
        char* set_point_time_1_hh_mm,
        char* set_point_time_2_hh_mm,
        char* set_point_time_3_hh_mm);
/*create iot message*/
// Function to handle the HTTP response from the server
static size_t write_callback(void *contents, size_t size, size_t nmemb, char *response_json);
bool create_iot_message(const char *url, const char *message, char *response_json);
/*handle commands in another thread*/
void* userInputThread(void* arg);
/*help menu*/
void help();
/*concat url, port, and function path & return web service URL*/
void make_url(const char* web_service_base_url, const char* port, const char* path, char** url_string);
/*set temp points*/
void parse_cmd_input_to_set_temp_points(const char* input, float* set_point_temp, char** set_point_time_hh_mm);
/*init_on_server*/
void init_on_server(); 
/*call the web services at http://20.163.108.241:5000 */
void call_web_service();
void printGlobalVariables();
/*
 * Function Declarations - end
 */

/*
 * Function Implementations - start
 */
void print_configs() {
    printf("Iot ID: %s\n", webServiceConfig.iot_id);
    printf("Web Service Base URL: %s\n", webServiceConfig.web_service_base_url);
    printf("Web Service Base Port: %s\n", webServiceConfig.web_service_base_port);
    printf("Create Config: %s\n", webServiceConfig.create_config);
    printf("Read Config: %s\n", webServiceConfig.read_config);
    printf("Update Config: %s\n", webServiceConfig.update_config);
    printf("Delete Config: %s\n", webServiceConfig.delete_config);
}

void printGlobalVariables() {
    printf("set_point_1_temp = %.2f\n", set_point_1_temp);
    printf("set_point_2_temp = %.2f\n", set_point_2_temp);
    printf("set_point_3_temp = %.2f\n", set_point_3_temp);
    printf("set_point_time_1_hh_mm = %s\n", set_point_time_1_hh_mm);
    printf("set_point_time_2_hh_mm = %s\n", set_point_time_2_hh_mm);
    printf("set_point_time_3_hh_mm = %s\n", set_point_time_3_hh_mm);
}

float read_current_temp(const char* path_to_temp) {
    float temp = FLT_MIN;
    if (path_to_temp != NULL) {
        FILE* file = fopen(path_to_temp, "r");
        if (file == NULL) {
            fprintf(stderr, "Error opening file: %s\n", path_to_temp);
            //TODO: log error msg 
        }
        if (fscanf(file, "%f", &temp) != 1) {
            fprintf(stderr, "Error reading temperature from file: %s\n", path_to_temp);
            fclose(file);
            //TODO: log error msg 
        }
    } else {
        printf("read_current_temp(), path_to_temp input is NULL\n");
    }
    return temp;
}

/*
 * float temp = read_current_temp(TEMP_PATH);
 * heat_toggle(HEATER_STATUS_PATH,ON);
   heat_toggle(HEATER_STATUS_PATH,OFF);
 */
void heat_toggle(const char* path, const char* status) {
    if ((path != NULL) && (status != NULL)) {
        FILE* file = fopen(path, "w");
        if (file == NULL) {
            printf("Error opening the file.\n");
            return;
        }

        char timestamp[20];
        get_utc_time(timestamp, sizeof (timestamp));
        printf("heater status=%c, timestamp=%s\n", *status, timestamp);
        fprintf(file, "%s:%s\n", status, timestamp);
        if (file != NULL) {
            fclose(file);
        }
    } else {
        printf("heat_toggle(), inputs are NULL\n");
    }
}

void get_utc_time(char *timestamp, size_t timestamp_size) {
    if (timestamp != NULL) {
        time_t now;
        struct tm *utc_time;
        now = time(NULL);
        utc_time = gmtime(&now);
        strftime(timestamp, timestamp_size, "%Y-%m-%d %H:%M:%S", utc_time);
    }
}

int isHeaterOn(const char* path) {
    int flag = -1;
    long file_size = 0;
    int is_empty = 0;
    if (path != NULL) {
        FILE* file = fopen(path, "r");
        if (file == NULL) {
            printf("Error opening the file.\n");
            flag = -1;
        }//end if 
        else {
            char line[48];
            char * p_file = fgets(line, sizeof (line), file);
            if (p_file != NULL) {
                //parse the line & split it based on ":"
                char* token = strtok(line, ";");
                if (token != NULL) {
                    if (token[0] == '1') {
                        flag = 1;
                    } else if (token[0] == '0') {
                        flag = 0;
                    } else {
                        flag = -1;
                    }
                }
            }//end line fgets
            else {
                //in case of empty file 
                flag = 0;
            }
            //close the file 
            if (file != NULL) {
                fclose(file);
            }
        }//end else 

    } else {
        printf("Error isHeaterOn() , input path is NULL.\n");
    }
    return flag;
}

// Function to read the JSON file and populate the global struct
bool readJSONConfig(const char *filepath) {
    FILE *file = fopen(filepath, "r");
    if (file == NULL) {
        printf("Error opening the file.\n");
        return 1;
    }

    // Get the file size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate memory for the buffer
    char *buffer = (char *) malloc(fileSize + 1); // +1 for null-terminator
    if (buffer == NULL) {
        printf("Memory allocation error.\n");
        fclose(file);
        return 1;
    }

    // Read the entire file into the buffer
    size_t bytesRead = fread(buffer, 1, fileSize, file);
    buffer[bytesRead] = '\0'; // Null-terminate the buffer
    // Parse the JSON buffer
    json_object *json = json_tokener_parse(buffer);
    free(buffer);
    fclose(file);

    // Retrieve values from JSON and populate the struct
    json_object_object_foreach(json, key, val) {
        if (strcmp(key, "web_service_base_url") == 0) {
            strncpy(webServiceConfig.web_service_base_url, json_object_get_string(val), sizeof (webServiceConfig.web_service_base_url) - 1);
            webServiceConfig.web_service_base_url[sizeof (webServiceConfig.web_service_base_url) - 1] = '\0';
        } else if (strcmp(key, "web_service_base_port") == 0) {
            strncpy(webServiceConfig.web_service_base_port, json_object_get_string(val), sizeof (webServiceConfig.web_service_base_port) - 1);
            webServiceConfig.web_service_base_port[sizeof (webServiceConfig.web_service_base_port) - 1] = '\0';
        } else if (strcmp(key, "create_config") == 0) {
            strncpy(webServiceConfig.create_config, json_object_get_string(val), sizeof (webServiceConfig.create_config) - 1);
            webServiceConfig.create_config[sizeof (webServiceConfig.create_config) - 1] = '\0';
        } else if (strcmp(key, "read_config") == 0) {
            strncpy(webServiceConfig.read_config, json_object_get_string(val), sizeof (webServiceConfig.read_config) - 1);
            webServiceConfig.read_config[sizeof (webServiceConfig.read_config) - 1] = '\0';
        } else if (strcmp(key, "update_config") == 0) {
            strncpy(webServiceConfig.update_config, json_object_get_string(val), sizeof (webServiceConfig.update_config) - 1);
            webServiceConfig.update_config[sizeof (webServiceConfig.update_config) - 1] = '\0';
        } else if (strcmp(key, "delete_config") == 0) {
            strncpy(webServiceConfig.delete_config, json_object_get_string(val), sizeof (webServiceConfig.delete_config) - 1);
            webServiceConfig.delete_config[sizeof (webServiceConfig.delete_config) - 1] = '\0';
        } else if (strcmp(key, "iot_id") == 0) {
            strncpy(webServiceConfig.iot_id, json_object_get_string(val), sizeof (webServiceConfig.iot_id) - 1);
            webServiceConfig.iot_id[sizeof (webServiceConfig.iot_id) - 1] = '\0';
        } else {

        }

    }

    return true;
}

/*construct the JSON message from input values*/
char* json_http_message(char* id, char* state, char* temp,
        float set_point_temp_1,
        float set_point_temp_2,
        float set_point_temp_3,
        char* set_point_time_1_hh_mm,
        char* set_point_time_2_hh_mm,
        char* set_point_time_3_hh_mm) {
             // Create a buffer to hold the updated iot_state_message
    const char* error_message = "{\"error\":\"could not build message\"}";
    char* iot_state_message = (char*) malloc(1000 * sizeof (char));
    if (id!=NULL && state!=NULL && temp!=NULL && set_point_time_1_hh_mm!=NULL && set_point_time_2_hh_mm!=NULL && set_point_time_3_hh_mm!=NULL)
    {

    if (iot_state_message == NULL) {
        printf("Memory allocation failed.\n");
        return NULL;
    }
    char temp1[16]={0,};
    char temp2[16]={0,};
    char temp3[16]={0,};
    snprintf(temp1, sizeof (temp1), "%.2f", set_point_temp_1);
    snprintf(temp2, sizeof (temp2), "%.2f", set_point_temp_2);
    snprintf(temp3, sizeof (temp3), "%.2f", set_point_temp_3);

    // Construct the JSON message with input values
    snprintf(iot_state_message, 1000,
            "{\n"
            "\t\"id\": \"%s\",\n"
            "\t\"state\": \"%s\",\n"
            "\t\"temp\": \"%s\",\n"
            "\t\"set_point_1_time_hh_mm\": \"%s\",\n"
            "\t\"set_point_2_time_hh_mm\": \"%s\",\n"
            "\t\"set_point_3_time_hh_mm\": \"%s\",\n"
            "\t\"set_point_1\": \"%s\",\n"
            "\t\"set_point_2\": \"%s\",\n"
            "\t\"set_point_3\": \"%s\"\n"
            "}",
            id, state, temp, set_point_time_1_hh_mm, set_point_time_2_hh_mm, set_point_time_3_hh_mm,
            temp1, temp2, temp3);
    }
    else
    {
        strcpy(iot_state_message, error_message);
    }
  
     return iot_state_message;
   
}

// Function to handle the HTTP response from the server
static size_t write_callback(void *contents, size_t size, size_t nmemb, char *response_json) {
    size_t total_size = size * nmemb;
    strcat(response_json, (char *) contents);
    return total_size;
}

// Function to create and post an IoT message to a given URL
bool create_iot_message(const char *url, const char *message, char *response_json) {
    bool success = false;

    // Initialize the libcurl library
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize libcurl\n");
        return false;
    }

    // Set up the POST request
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, message);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_json);

    // Perform the HTTP POST request
    CURLcode res = curl_easy_perform(curl);

    // Check for errors and get the HTTP response code
    long http_response_code = 0;
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_response_code);
        printf("{\"http_response\": %ld}", http_response_code);

        // Check if the HTTP response code is 200 (OK)
        if (http_response_code == 200) {
            success = true;
        }
    } else {
        fprintf(stderr, "HTTP request failed: %s\n", curl_easy_strerror(res));
    }

    // Clean up and release resources
    curl_easy_cleanup(curl);

    return success;
}

/*
 * process reads the temp & turns heater on or off based on the setpoint. 
 */
int process(int argc, char** argv) {
    int program_status = 0;
    printf("Starting IoT Client...");
    help();
    //process user requests from command line in another thread without blocking 
    pthread_t tid;
    if (argc < 2) {
        printf("IoT Client, missing required inputs!");
    } else {
        
        //1) start a non blocking thread to handle user input 
        int threadCreationResult = pthread_create(&tid, NULL, userInputThread, NULL);
        if (threadCreationResult != 0) {
            fprintf(stderr, "Error creating thread.\n");
            return 1;
        }


        float current_temp = FLT_MIN;
        float current_set_point_temp = 100;
        int heater_status = FLT_MIN;
        bool read_settings = false;

        //2) read required settings from JSON 
        read_settings = readJSONConfig(argv[2]);
        if (read_settings) {

            if (argc >= 2 && strcmp(argv[1], "1") == 0) {
                CONTINUE = 1;
                init_on_server();

            } else {
                //TODO: log message 
                printf("Invalid input. Usage: %s 1\n", argv[0]);
                program_status = 1;
                return program_status;
            }


            int user_input;
            //while loop
            while (1) {
                if (CONTINUE) {
                    //1) read command line & if status is ON=1 continue else OFF=0
                    current_temp = read_current_temp(TEMP_PATH);
                    //2) check the temp. if temp => set_point then HEATER = OFF else heater ON. 
                    if (current_temp < current_set_point_temp) {
                        //only turn on if heater is not already on
                        heater_status = isHeaterOn(HEATER_STATUS_PATH);
                        if (heater_status == 0) {
                            //turn on 
                            heat_toggle(HEATER_STATUS_PATH, HEATER_ON);
                            //TODO: call web service 
                        } else if (heater_status == 1) {
                            //no action, already ON=1
                            //printf("%s",NO_ACTION_ON);
                            log_message(STATUS_PATH, NO_ACTION_ON);
                        } else {
                            //no action
                            continue;
                        }
                    }// if temp > set point 
                    else {
                        //only turn off if heater is not already off
                        heater_status = isHeaterOn(HEATER_STATUS_PATH);
                        if (heater_status == 1) {
                            //turn off 
                            heat_toggle(HEATER_STATUS_PATH, HEATER_OFF);
                            //TODO: call web service 
                        } else if (heater_status == 0) {
                            //no action, already OFF=0
                            //printf("%s",NO_ACTION_OFF);
                            log_message(STATUS_PATH, NO_ACTION_OFF);
                        } else {
                            //no action
                            continue;
                        }
                    }//end if temp > set point 
                    sleep(1);
                }//end if CONTINUE


            }//end while loop 


            //TODO: log 
            printf("Heater Monitoring is shutdown.");
        } else {
            //TODO: log 
            printf("Error: could not log configurations");
        }


    }//end else has requirec inputs


    return program_status;
}//end process

void parse_cmd_input_to_set_temp_points(const char* input, float* set_point_temp, char** set_point_time_hh_mm) {
    float temp_flt;
    char hour[3];
    char min[3];
    bool ok_temp = false;
    bool ok_hour = false;
    bool ok_min = false;

    if (input != NULL && *set_point_time_hh_mm != NULL) {
        int items_matched = sscanf(input, "%f,%2[^:]:%2s", &temp_flt, hour, min);
        if (items_matched != 3) {
            printf("Invalid format.\n");
            help();
            return;
        }

        if (temp_flt < 0 || temp_flt > 100) {
            printf("Temperature must be between 0 to 100.\n");
            return;
        } else {
            ok_temp = true;
        }

        int hour_val = atoi(hour);
        if (hour_val < 0 || hour_val > 23) {
            printf("Invalid hour. The hour must be between 0 to 23.\n");
        } else {
            ok_hour = true;
        }

        int min_val = atoi(min);
        if (min_val < 0 || min_val > 59) {
            printf("Invalid minutes. The minutes must be between 0 to 59.\n");
        } else {
            ok_min = true;
        }

        if (ok_temp && ok_hour && ok_min) {
            *set_point_temp = temp_flt;

            // Replace the "HH" part in the global variable set_point_time_1_hh_mm with hour
            set_point_time_hh_mm[0] = (char)('0'+(hour_val / 10));
            set_point_time_hh_mm[1] =  (char)('0'+(hour_val % 10));

            // Replace the "MM" part in the global variable set_point_time_1_hh_mm with min
            set_point_time_hh_mm[3]  =(char) ('0'+(min_val / 10));
            set_point_time_hh_mm[4] = (char)('0'+(min_val % 10));
        }
    } else {
        printf("parseCommandLineInput() invalid input!");
    }

}

/*userInputThread is executed by the separate thread, so we can continue to send commands*/
void* userInputThread(void* arg) {
    char input[MAX_INPUT_LENGTH_1];
    char input_2[MAX_INPUT_LENGTH_2];
    float current_temp = FLT_MIN;
    char* url = NULL; 
    bool call_webservice = false;
    bool exit_flag = false; 
    while (1) {
        printf("Enter input (or 'exit' to quit): ");
        fgets(input, MAX_INPUT_LENGTH_1, stdin);
        //process the user input 
        printf("User input: %s", input);
        if (strcmp(input, "1\n") == 0) {
            printf("starting/re-starting...\n");
            CONTINUE = 1;
            int total_url_length = strlen(webServiceConfig.web_service_base_url) + strlen(webServiceConfig.web_service_base_port) + strlen(webServiceConfig.update_config);
            url = (char*) malloc(total_url_length + 1);
            make_url(webServiceConfig.web_service_base_url, webServiceConfig.web_service_base_port, webServiceConfig.create_config, &url);
            call_webservice = true;

        }
        if (strcmp(input, "2\n") == 0) {
            printf("stopping...\n");
            CONTINUE = 0;
            int total_url_length = strlen(webServiceConfig.web_service_base_url) + strlen(webServiceConfig.web_service_base_port) + strlen(webServiceConfig.update_config);
            url = (char*) malloc(total_url_length + 1);
            make_url(webServiceConfig.web_service_base_url, webServiceConfig.web_service_base_port, webServiceConfig.create_config, &url);
            call_webservice = true;

        }
        if (strcmp(input, "3\n") == 0) {
            current_temp = read_current_temp(TEMP_PATH);
            char temp[16]; // A buffer to store the resulting string
            snprintf(temp, sizeof (temp), "%.2f", current_temp);
            printf("%s Celsius\n", temp);

        }
        if (strcmp(input, "4\n") == 0) {
            printf("Setting the temperature point 1. Enter temp,HH:MM.\n");
            fgets(input, MAX_INPUT_LENGTH_2, stdin);
            //set temp point 1 
            parse_cmd_input_to_set_temp_points(input, &set_point_1_temp, &set_point_time_1_hh_mm);

        }
        if (strcmp(input, "5\n") == 0) {
            printf("Setting the temperature point 2. Enter temp,HH:MM.\n");
            fgets(input, MAX_INPUT_LENGTH_2, stdin);
            parse_cmd_input_to_set_temp_points(input, &set_point_2_temp, &set_point_time_2_hh_mm);

        }
        if (strcmp(input, "6\n") == 0) {
            printf("Setting the temperature point 3. Enter temp,HH:MM.\n");
            fgets(input, MAX_INPUT_LENGTH_2, stdin);
            parse_cmd_input_to_set_temp_points(input, &set_point_3_temp, &set_point_time_3_hh_mm);

        }
        if (strcmp(input, "7\n") == 0) {
            printf("Get all temperature settings.\n");
            printGlobalVariables();

        }
        if (strcmp(input, "-h\n") == 0 || strcmp(input, "-help\n") == 0) {
            help();

        }
        if (strcmp(input, "-e\n") == 0 || strcmp(input, "-exit\n") == 0) {
            printf("Exiting...\n");
            CONTINUE = 0;
            int total_url_length = strlen(webServiceConfig.web_service_base_url) + strlen(webServiceConfig.web_service_base_port) + strlen(webServiceConfig.update_config);
            url = (char*) malloc(total_url_length + 1);
            make_url(webServiceConfig.web_service_base_url, webServiceConfig.web_service_base_port, webServiceConfig.create_config, &url);
            call_webservice = true;
            exit_flag= true; 
            break;
        }
        //if call_webservice?
        if (call_webservice)
        {
            //construct web service call - start 
            current_temp = read_current_temp(TEMP_PATH);
            char temp[16]; // A buffer to store the resulting string
            snprintf(temp, sizeof (temp), "%.2f", current_temp);
            char* state = "0";
            if (CONTINUE) {
                state = "1";
            }
            char* iot_msg = json_http_message(webServiceConfig.iot_id, state, temp, set_point_1_temp, set_point_2_temp, set_point_3_temp, set_point_time_1_hh_mm, set_point_time_2_hh_mm, set_point_time_3_hh_mm);
            char response_json[512] = {0};
            log_message(LOG_MESSAGES_PATH, url);
            printf("Calling=%s\n:", url);
            log_message(LOG_MESSAGES_PATH, iot_msg);
            printf("Payload=%s\n:", iot_msg);
            bool result = create_iot_message(url, iot_msg, response_json);
            if (result) {
                printf("Message sent successfully!\n");
                printf("HTTP Response JSON: %s\n", response_json);
                log_message(LOG_MESSAGES_PATH, response_json);
            } else {
                printf("Failed to send message.\n");
                log_message(LOG_MESSAGES_PATH, "Failed to send message.\n");
            }
            call_webservice = false; 
        }//end if 
        
         
    }//end while
    //clean the thread
    pthread_exit(NULL);
    if (exit_flag)
    {
        exit(0);
    }
}

/*helper to concat the */
void make_url(const char* web_service_base_url, const char* port, const char* path, char** url_string) {
    if (web_service_base_url != NULL && port != NULL && path != NULL && *url_string != NULL) {
        //copy sub strings
        strcpy(*url_string, web_service_base_url);
        strcat(*url_string, ":");
        strcat(*url_string, port);
        strcat(*url_string, path);

    }

}

/*log messages to log files*/
void log_message(const char* path_to_log, const char* msg) {
    // Open the log file in "append" mode, so new messages are appended to the end.
    FILE* logFile = fopen(path_to_log, "a");

    if (logFile == NULL) {
        fprintf(stderr, "Error opening the log file: %s\n", path_to_log);
        return;
    }

    // Write the message to the log file.
    fprintf(logFile, "%s\n", msg);

    // Close the file when done.
    fclose(logFile);
}

/*create the IoT device on the server*/
void init_on_server()
{
        float current_temp = FLT_MIN;
        char* url = NULL; 
        printf("create IoT device record on the server...");
        CONTINUE = 1;
        int total_url_length = strlen(webServiceConfig.web_service_base_url) + strlen(webServiceConfig.web_service_base_port) + strlen(webServiceConfig.create_config);
        url = (char*) malloc(total_url_length + 1);
        make_url(webServiceConfig.web_service_base_url, webServiceConfig.web_service_base_port, webServiceConfig.create_config, &url);             
        //construct web service call - start 
                char temp[16]; // A buffer to store the resulting string
                snprintf(temp, sizeof (temp), "%.2f", current_temp);
                char* state = "0";
                if (CONTINUE)
                {
                    state = "1";
                }
                char* iot_msg = json_http_message(webServiceConfig.iot_id,state, temp, set_point_1_temp, set_point_2_temp, set_point_3_temp, set_point_time_1_hh_mm, set_point_time_2_hh_mm, set_point_time_3_hh_mm);
                char response_json[512] = {0};
                log_message(LOG_MESSAGES_PATH, url);
                printf("Calling%s\n:", url);
                log_message(LOG_MESSAGES_PATH, iot_msg);
                printf("Payload%s\n:", iot_msg);
                bool result = create_iot_message(url, iot_msg, response_json);
                if (result) {
                    printf("Message sent successfully!\n");
                    printf("HTTP Response JSON: %s\n", response_json);
                    log_message(LOG_MESSAGES_PATH, response_json);
                } else {
                    printf("Failed to send message.\n");
                    log_message(LOG_MESSAGES_PATH, "Failed to send message.\n");
                }

}

/*help menu*/
void help() {
    printf("Heater & Temp Control:\n");
    printf("1) start/restart heater - Input 1, the press Enter\n");
    printf("2) stop heater - Input 2\n");
    printf("3) current temp - Input 3\n");
    printf("4) set temperature point 1 & time - Input 4 press Enter,then temp,hour:min press Enter\n");
    printf("5) set temperature point 2 & time - Input 5 press Enter,then temp,hour:min press Enter\n");
    printf("6) set temperature point 3 & time - Input 6 press Enter,then temp,hour:min press Enter\n");
    printf("7) get all temperature points - Input 7\n");
    printf("8) Help - Input -h or -help\n");
    printf("9)) Exit/Shutdown - Input -e or -exit\n");
}

/* Function implementations - End*/


/* MAIN*/
int main(int argc, char** argv) {
    process(argc, argv);
    return (EXIT_SUCCESS);
}
