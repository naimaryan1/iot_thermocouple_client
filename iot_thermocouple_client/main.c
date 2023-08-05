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
 * 7) Make the <your_path>/thermocouple/tcsimd is running 
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <float.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <pthread.h>
#include <unistd.h>
#include <cjson/cJSON.h>
#include <cjson/cJSON_Utils.h>
#define MAX_INPUT_LENGTH_1 4
#define MAX_INPUT_LENGTH_2 11
#define MAX_JSON_FILE_SIZE_BYTES 3072
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


//struct to store config_json values that passed in via cmd 
//lets preallocate to avoid calling malloc mid program 

typedef struct WebServiceConfig {
    char web_service_base_url[2048];
    char web_service_base_port[6];
    char create_config[64];
    char read_config[64];
    char update_config[64];
    char delete_config[64];
    char iot_id[16];
    char entire_json_string[MAX_JSON_FILE_SIZE_BYTES];
} WebServiceConfig;
WebServiceConfig webServiceConfig;

typedef struct SetPoints {
    float set_point_1_temp;
    float set_point_2_temp;
    float set_point_3_temp;
    char* set_point_time_1_hh_mm;
    char* set_point_time_2_hh_mm;
    char* set_point_time_3_hh_mm;
} SetPoints;
SetPoints setPoints;
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
int process(int argc, char** argv, WebServiceConfig* ws_config, SetPoints* set_points);
/*isHeaterOn*/
int isHeaterOn(const char* path);
/*read JSON from path*/
/*References:http://json-c.github.io/json-c/json-c-current-release/doc/html/index.html*/
int readJSONConfig(const char *filepath, WebServiceConfig* ws_config);
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
void parse_cmd_input_to_set_temp_points(char* input, int point, SetPoints* set_points);
/*init_on_server*/
void init_on_server();
/*call the web services at http://20.163.108.241:5000 */
void call_web_service();
void printGlobalVariables(SetPoints* set_points);
void init(WebServiceConfig* wb_service_config, SetPoints* set_points);
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

void printGlobalVariables(SetPoints* set_points) {
    if (NULL != set_points) {
        printf("set_point_1_temp = %.2f\n", set_points->set_point_1_temp);
        printf("set_point_2_temp = %.2f\n", set_points->set_point_2_temp);
        printf("set_point_3_temp = %.2f\n", set_points->set_point_3_temp);
        printf("set_point_time_1_hh_mm = %s\n", set_points->set_point_time_1_hh_mm);
        printf("set_point_time_2_hh_mm = %s\n", set_points->set_point_time_2_hh_mm);
        printf("set_point_time_3_hh_mm = %s\n", set_points->set_point_time_3_hh_mm);
    }

}

float read_current_temp(const char* path_to_temp) {
    float temp = FLT_MIN;
    if (path_to_temp != NULL) {
        FILE* file = fopen(path_to_temp, "r");
        if (file == NULL) {
            fprintf(stderr, "Error opening file: %s\n", path_to_temp);
            //TODO: log error msg 
        } else {
            if (fscanf(file, "%f", &temp) != 1) {
                fprintf(stderr, "Error reading temperature from file: %s\n", path_to_temp);
                fclose(file);
                //TODO: log error msg 
            }
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

int readJSONConfig(const char* filepath, WebServiceConfig* ws_config) {
    /*
    {
    "iot_id": "1000",
    "web_service_base_url": "http://20.163.108.241",
    "web_service_base_port": "5000",
    "create_config": "/create_iot_device",
    "read_config": "/read_iot_device",
    "update_config": "/update_iot_device",
    "delete_config": "/delete_iot_device"
     }
     */
    int flag = 0;
    if (NULL != filepath && NULL != ws_config) {
        FILE* fp = fopen(filepath, "r");

        if (!fp) {
            printf("Error opening the file.\n");
            flag = 0;
        } else {
            //init the string 
            memset(ws_config->entire_json_string, '\0', sizeof (ws_config->entire_json_string));
            //the the file contents to the the string 
            fread(ws_config->entire_json_string, 1, MAX_JSON_FILE_SIZE_BYTES, fp);
            cJSON* root = cJSON_Parse(ws_config->entire_json_string);
            //close the file
            fclose(fp);
            //do we have a valid root?
            if (!root) {
                printf("JSON parsing failed.\n");
                flag = 0;
            } else {
                //iot_id
                const char* iot_id_key = "iot_id";
                cJSON* iot_val = cJSON_GetObjectItemCaseSensitive(root, iot_id_key);
                memset(ws_config->iot_id, '\0', sizeof (ws_config->iot_id));
                strncpy(ws_config->iot_id, iot_val->valuestring, strlen(iot_val->valuestring));
                //web_service_base_url
                const char* web_service_base_url_key = "web_service_base_url";
                cJSON* url_val = cJSON_GetObjectItemCaseSensitive(root, web_service_base_url_key);
                memset(ws_config->web_service_base_url, '\0', sizeof (ws_config->web_service_base_url));
                strncpy(ws_config->web_service_base_url, url_val->valuestring, strlen(url_val->valuestring));
                //web_service_base_port
                const char* web_service_port_key = "web_service_base_port";
                cJSON* port_val = cJSON_GetObjectItemCaseSensitive(root, web_service_port_key);
                memset(ws_config->web_service_base_port, '\0', sizeof (ws_config->web_service_base_port));
                strncpy(ws_config->web_service_base_port, port_val->valuestring, strlen(port_val->valuestring));
                //create_config
                const char* create_config_key = "create_config";
                cJSON* create_config_val = cJSON_GetObjectItemCaseSensitive(root, create_config_key);
                memset(ws_config->create_config, '\0', sizeof (ws_config->create_config));
                strncpy(ws_config->create_config, create_config_val->valuestring, strlen(create_config_val->valuestring));
                //read
                const char* read_config_key = "read_config";
                cJSON* read_config_val = cJSON_GetObjectItemCaseSensitive(root, read_config_key);
                memset(ws_config->read_config, '\0', sizeof (ws_config->read_config));
                strncpy(ws_config->read_config, read_config_val->valuestring, strlen(read_config_val->valuestring));
                //update
                const char* update_config_key = "update_config";
                cJSON* update_config_val = cJSON_GetObjectItemCaseSensitive(root, update_config_key);
                memset(ws_config->update_config, '\0', sizeof (ws_config->update_config));
                strncpy(ws_config->update_config, update_config_val->valuestring, strlen(update_config_val->valuestring));
                //delete
                const char* delete_config_key = "delete_config";
                cJSON* delete_config_val = cJSON_GetObjectItemCaseSensitive(root, delete_config_key);
                memset(ws_config->delete_config, '\0', sizeof (ws_config->delete_config));
                strncpy(ws_config->delete_config, delete_config_val->valuestring, strlen(delete_config_val->valuestring));
                flag = 1;
                //makesure to cleanup
                free(root);
            }//end else 
        }

    } else {
        printf("Error readJSONConfig(), input params filerpath and/or ws_config are NUJLL\n");

    }

    return flag;
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
    if (id != NULL && state != NULL && temp != NULL && set_point_time_1_hh_mm != NULL && set_point_time_2_hh_mm != NULL && set_point_time_3_hh_mm != NULL) {

        if (iot_state_message == NULL) {
            printf("Memory allocation failed.\n");
            return NULL;
        }
        char temp1[16] = {0,};
        char temp2[16] = {0,};
        char temp3[16] = {0,};
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
    } else {
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
    curl_global_init(CURL_GLOBAL_ALL);
    // Initialize the libcurl library
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize libcurl\n");
        return false;
    } else {

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        // Set up the POST request
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, message);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long) strlen(message));
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
    }

    curl_global_cleanup();
    return success;
}

/*
 * process reads the temp & turns heater on or off based on the setpoint. 
 */
int process(int argc, char** argv, WebServiceConfig* ws_config, SetPoints* set_points) {

    int program_status = 0;
    printf("Starting IoT Client...");
    //alloc & remember to free 
    if (NULL == setPoints.set_point_time_1_hh_mm) {

        setPoints.set_point_time_1_hh_mm = (char*) calloc(10, sizeof (char));
        setPoints.set_point_time_2_hh_mm = (char*) calloc(10, sizeof (char));
        setPoints.set_point_time_3_hh_mm = (char*) calloc(10, sizeof (char));
    }
    //init the chars 
    if (NULL != setPoints.set_point_time_1_hh_mm) {
        memset(setPoints.set_point_time_1_hh_mm, '\0', sizeof (setPoints.set_point_time_1_hh_mm));
        memset(setPoints.set_point_time_2_hh_mm, '\0', sizeof (setPoints.set_point_time_2_hh_mm));
        memset(setPoints.set_point_time_3_hh_mm, '\0', sizeof (setPoints.set_point_time_3_hh_mm));
    }
    init(ws_config, set_points);
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
        read_settings = readJSONConfig(argv[2], ws_config);
        print_configs();
        if (read_settings) {

            if (argc >= 2 && (strcmp(argv[1], "1")) == 0) {
                CONTINUE = 1;
                init_on_server();

            } else {
                //TODO: log message 
                printf("Invalid input. Usage: %s 1\n", argv[0]);
                program_status = 1;
                return program_status;
            }


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

    //clean 
    if (NULL != setPoints.set_point_time_1_hh_mm) {
        free(setPoints.set_point_time_1_hh_mm);
        free(setPoints.set_point_time_2_hh_mm);
        free(setPoints.set_point_time_3_hh_mm);

    }
    return program_status;
}//end process

void parse_cmd_input_to_set_temp_points(char* input, int point, SetPoints* set_points) {
    char *token;
    float temp = 0.0;
    float hours = 0;
    float min = 0;
    int match = 0;
    // Remove the newline character from the end of the input
    int length = strlen(input);
    if (length >= 8) {
        input[length - 1] = '\0';
        char temp_array[3];
        memset(temp_array, '\0', sizeof (temp_array));
        token = strtok(input, ",");
        if (token) {
            strcpy(temp_array, token);
            match = sscanf(temp_array, "%3f", &temp);

            if (match == 1) {

                char hours_min[6];
                hours_min[6] = '\0';
                memset(hours_min, '\0', sizeof (hours_min));
                if (point == 1) {
                    set_points->set_point_1_temp = temp;
                    if (length == 9) {
                        strncpy(hours_min, input + 3, 5);
                        //set 
                        strncpy(set_points->set_point_time_1_hh_mm, hours_min, strlen(hours_min));
                    }
                    if (length == 10) {
                        set_points->set_point_1_temp = temp;
                        strncpy(hours_min, input + 4, 5);
                        strncpy(set_points->set_point_time_1_hh_mm, hours_min, strlen(hours_min));
                    }
                }
                if (point == 2) {
                    set_points->set_point_2_temp = temp;
                    if (length == 9) {
                        strncpy(hours_min, input + 3, 5);
                        strncpy(set_points->set_point_time_2_hh_mm, hours_min, strlen(hours_min));
                    }
                    if (length == 10) {
                        strncpy(hours_min, input + 4, 5);
                        strncpy(set_points->set_point_time_2_hh_mm, hours_min, strlen(hours_min));
                    }
                }
                if (point == 3) {
                    set_points->set_point_3_temp = temp;
                    if (length == 9) {
                        strncpy(hours_min, input + 3, 5);
                        strncpy(set_points->set_point_time_3_hh_mm, hours_min, strlen(hours_min));

                    }
                    if (length == 10) {

                        strncpy(hours_min, input + 4, 5);
                        strncpy(set_points->set_point_time_3_hh_mm, hours_min, strlen(hours_min));

                    }
                }

            }

        } else {
            printf("Bad format must be temp,HH:MM. You forgot the commna.\n");

        }
    } else {
        printf("Bad format must be temp,HH:MM\n");
    }

}

void parse_cmd_input_set_hour_min(char* input, SetPoints* set_points);

/*userInputThread is executed by the separate thread, so we can continue to send commands*/
void* userInputThread(void* arg) {
    char input[MAX_INPUT_LENGTH_1] = {0};
    char input_2[MAX_INPUT_LENGTH_2] = {0};

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
            make_url(webServiceConfig.web_service_base_url, webServiceConfig.web_service_base_port, webServiceConfig.update_config, &url);
            call_webservice = true;

        }
        if (strcmp(input, "2\n") == 0) {
            printf("stopping...\n");
            CONTINUE = 0;
            int total_url_length = strlen(webServiceConfig.web_service_base_url) + strlen(webServiceConfig.web_service_base_port) + strlen(webServiceConfig.update_config);
            url = (char*) malloc(total_url_length + 1);
            make_url(webServiceConfig.web_service_base_url, webServiceConfig.web_service_base_port, webServiceConfig.update_config, &url);
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
            fgets(input_2, MAX_INPUT_LENGTH_2, stdin);
            //set temp point 1 
            parse_cmd_input_to_set_temp_points(input_2, 1, &setPoints);
            int total_url_length = strlen(webServiceConfig.web_service_base_url) + strlen(webServiceConfig.web_service_base_port) + strlen(webServiceConfig.update_config);
            url = (char*) malloc(total_url_length + 1);
            make_url(webServiceConfig.web_service_base_url, webServiceConfig.web_service_base_port, webServiceConfig.update_config, &url);
            call_webservice = true;

        }
        if (strcmp(input, "5\n") == 0) {
            printf("Setting the temperature point 2. Enter temp,HH:MM.\n");
            fgets(input_2, MAX_INPUT_LENGTH_2, stdin);
            parse_cmd_input_to_set_temp_points(input_2, 2, &setPoints);
            int total_url_length = strlen(webServiceConfig.web_service_base_url) + strlen(webServiceConfig.web_service_base_port) + strlen(webServiceConfig.update_config);
            url = (char*) malloc(total_url_length + 1);
            make_url(webServiceConfig.web_service_base_url, webServiceConfig.web_service_base_port, webServiceConfig.update_config, &url);
            call_webservice = true;

        }
        if (strcmp(input, "6\n") == 0) {
            printf("Setting the temperature point 3. Enter temp,HH:MM.\n");
            fgets(input_2, MAX_INPUT_LENGTH_2, stdin);
            parse_cmd_input_to_set_temp_points(input_2, 3, &setPoints);
            int total_url_length = strlen(webServiceConfig.web_service_base_url) + strlen(webServiceConfig.web_service_base_port) + strlen(webServiceConfig.update_config);
            url = (char*) malloc(total_url_length + 1);
            make_url(webServiceConfig.web_service_base_url, webServiceConfig.web_service_base_port, webServiceConfig.update_config, &url);
            call_webservice = true;

        }
        if (strcmp(input, "7\n") == 0) {
            printf("Get all temperature settings.\n");
            printGlobalVariables(&setPoints);

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
            exit_flag = true;
        }
        //if call_webservice?
        if (call_webservice) {
            //construct web service call - start 
            current_temp = read_current_temp(TEMP_PATH);
            char temp[16]; // A buffer to store the resulting string
            snprintf(temp, sizeof (temp), "%.2f", current_temp);
            char* state = "0";
            if (CONTINUE) {
                state = "1";
            }
            char* iot_msg = json_http_message(webServiceConfig.iot_id, state, temp, setPoints.set_point_1_temp, setPoints.set_point_2_temp, setPoints.set_point_3_temp, setPoints.set_point_time_1_hh_mm, setPoints.set_point_time_2_hh_mm, setPoints.set_point_time_3_hh_mm);
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
        if (exit_flag)
        {
            break; 
        }
    }//end while
    //clean the thread
    pthread_exit(NULL);
    if (exit_flag) {
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
void init_on_server() {
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
    if (CONTINUE) {
        state = "1";
    }
    char* iot_msg = json_http_message(webServiceConfig.iot_id, state, temp, setPoints.set_point_1_temp, setPoints.set_point_2_temp, setPoints.set_point_3_temp, setPoints.set_point_time_1_hh_mm, setPoints.set_point_time_2_hh_mm, setPoints.set_point_time_3_hh_mm);
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

void init(WebServiceConfig* wb_service_config, SetPoints* set_points) {
    //init web service struct 
    webServiceConfig.iot_id[0] = '\0';
    webServiceConfig.web_service_base_url[0] = '\0';
    webServiceConfig.web_service_base_port[0] = '\0';
    webServiceConfig.create_config[0] = '\0';
    webServiceConfig.read_config[0] = '\0';
    webServiceConfig.update_config[0] = '\0';
    webServiceConfig.delete_config[0] = '\0';
    //init set points struct 
    set_points->set_point_1_temp = 0;
    set_points->set_point_2_temp = 0;
    set_points->set_point_3_temp = 0;
    set_points->set_point_time_1_hh_mm[0] = '\0';
    set_points->set_point_time_2_hh_mm [0] = '\0';
    set_points->set_point_time_3_hh_mm [0] = '\0';
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
    printf("8) For help menu input -h or -help\n");
    printf("9) Exit/Shutdown - Input -e or -exit\n");
}

/* Function implementations - End*/

/* MAIN*/
int main(int argc, char** argv) {
    process(argc, argv, &webServiceConfig, &setPoints);
    return (EXIT_SUCCESS);
}
