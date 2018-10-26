 /*
 CITS2002 Project 1 2017
 Name:             Andrew Davis
 Student number:   20743192
 Date:                22/09/2017
 */

/*
 Compile with: cc -std=c99 -Wall -pedantic -Werror -o wifistats wifistats.c
 Usage: ./wifistats t|r packetfile [OUIfile]
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

/* These constant have been provided to make it easier to change the code in the case of the parameters changing such as maximum number of MAC addresses or the vendor name length. They also allow for code to be less error prone by providing names for frequently used constants around how Mac addresses and OUIs are formatted */
//Constant with the maximum number of MAC adresses the program is built to handle
#define MAX_MAC 500
//Constant with the maximum number of OUIs
#define MAX_OUI 25000
//Constant with the maximum length a vendors name can be
#define MAX_VENDOR_CHAR 90
//Constant with the number of bytes in a MAC address
#define MAC_AD_BYTES 6
//Constant with the length of a string containing the MAC address
#define MAC_CHAR_LEN ( (MAC_AD_BYTES * 2) + (MAC_AD_BYTES - 1) )
//Constant with the number of bytes holding the OUI portion of the MAC address
#define OUI_BYTES 3
//Constant with the length of a string to hold the OUI portion of a MAC address
#define OUI_CHAR_LEN ( (OUI_BYTES * 2) + (OUI_BYTES - 1) )

/*This is the function to sort the output and is called by the child process only */
void startSort (int sortCol) {
    /* This creates an array with the necessary parameters to call sort through execv. The array is similar to argv[] from main and is structured that way with the process called as the first string followed by the argument strings to be given to the new process terminating in a NULL pointer */
    char *newargs[] = {
        //The process being called
        "sort",
        //The first argument string, this one tells sort that the fields are seperated by a tab character
        "-t\t",
        //These next two arguments are left blank to be filled in later depending of the field that needs to be sorted
        "",
        "",
        //This argument supplies which file is to be sorted
        "output",
        //The NULL pointer to terminate this array
        NULL
    };
    //Checks to see which field to sort by.
    //If there are 2 fields
    if (sortCol == 2) {
        //sort by the second field containing the packet sizes numerically and reverse order
        newargs[2] = "-k2rn";
        //if there is a tie in the sorting sort by the first field, the MAC address
        newargs[3] = "-k1";
    }
    //If there are 3 fields
    else if (sortCol == 3) {
        //sort by the third field containing the packet sizes numerically and reverse order
        newargs[2] = "-k3rn";
        //if there is a tie in the sorting sort by the second field the vendor names, in ascending alphabetical order
        newargs[3] = "-k2";
    }
    //If the incorrect paramenters are supplied terminate program
    else {
        printf("Cannot sort file, fields are wrong\n");
        exit(EXIT_FAILURE);
    }
    /* Calls execve through execv with the process name and the array of arguments. execv is a wrapper for execve and provides the environment array automatically */
    execv("/usr/bin/sort", newargs);
    //if the child process gets to here execv failed and the process exits with a failure
    perror("execve of /usr/bin/sort failed");
    exit(EXIT_FAILURE);
}
/* This function is the beginning of the sorting. It forks the process and then get the child process to sort while the parent waits for the child to finish */
int sortoutput (int sortCol) {
    //This is a variable for the pid to determine the parent and child processes
    pid_t pid;
    //This is a status variable so that the parent can wait until the child is finished
    int status;
    
    /* This switch statement it to work out which process is the parent and which is the child. It calls fork and assigns the return value to pid */
    switch (pid = fork()) {
        //If fork returns a -1 it means the fork failed and the appropriate error message is printed before the process exits
        case -1:
            perror("fork() failed");
            exit(EXIT_FAILURE);
            break;
        //If fork returns 0 the the process is the child process
        case 0:
            //The child process calls the sort function
            startSort(sortCol);
            break;
        //If fork is not -1 or 0 then it is the parent process
        default:
            //The parent process waits for the child process to finish before continuing on
            wait(&status);
            //The output file which was used by sort to read in the data to sort gets deleted to clean up before the program exits.
            if (remove("output") != 0) {
                //If it can't be deleted then an error is printed before the program terminates as a failure
                perror("Cannot delete file output");
                exit(EXIT_FAILURE);
            }
            else {
                //Return EXIT_SUCCESS (0) to indicate success
                return EXIT_SUCCESS;
            }
    }
    //If the process makes it to here something has gone wrong and it returns EXIT_FAILURE to indicate this
    return EXIT_FAILURE;
}
/* This is the function that analyses and collates the MAC addresses that are the same. It takes in char mode to check whether to look at transmitting or recieving MAC addresses and packetsName, the file that contains the MAC addresses and packet sizes. This function returns an int of 0 to indicate success if managed to execute successfully */
int analyseMacAd(char mode, char packetsName[]) {
    //This is to hold the stream to the packet file
    FILE *packets;
    //This is to hold the stream to output file where the results are printed to
    FILE *output;
    //This is an array to read in each line of the file, the size is set to the size of the input buffer
    char line[BUFSIZ];
    //This is a string to read in the recieving MAC addresses set to the length of a MAC address
    char rAd[MAC_CHAR_LEN];
    //This is a string to read in the transmitting MAC addresses set to the length of a MAC address
    char tAd[MAC_CHAR_LEN];
    //This is an array to read in the individual bytes of the MAC address and is set to the number of bytes in the MAC address
    int checkAd[MAC_AD_BYTES];
    //This is an array to hold the proccessed MAC addresses and packet information, it is structured with it's length being the maximum number of MAC addresses the program is built to handle and the individual bytes of the MAC address for the first 6 values plus the packet information on the end
    int macAdList[MAX_MAC][MAC_AD_BYTES + 1];
    //This is an int to hold the packet size data for the MAC address being looked at
    int packetSiz = 0;
    //This is an int holding the number of MAC addresses in the macAdList array, this is different to the number of MAC addresses that have been processed as this looks at unique MAC addresses
    int noAd = 0;
    //Opening the packets file, supplying the file name and opening mode to fopen, in this case opening the file for reading only
    packets = fopen(packetsName, "r");
    //Checking to see if the file has been opened successfully, if not it'll return a NULL pointer
    if (packets == NULL) {
        //Print out the error information before terminating the process as a failure
        perror("Cannot open packets file");
        exit(EXIT_FAILURE);
    }
    //Creating the file to print the results to, the file is called output and is being created thought the w+ parameter, if an existing file with the name output exists it is overwritten
    output = fopen("output", "w+");
    //Checking to see if the file has been opened successfully, if not it'll return a NULL pointer
    if (output == NULL) {
        //Print out the error information before terminating the process as a failure
        perror("Cannot proccess output file");
        exit(EXIT_FAILURE);
    }
    //This is to read in each line from the packets file, it loops over each line scanning them into line and then processing the contents of line
    while (fgets(line, sizeof line, packets) != NULL) {
        //This breaks down line into 4 parts, the first part is ignored, then two strings are scanned into tAd and rAd respectivly seperated by a whitespace character, in this case \t and finally the last part is scanned into packetSiz as an integer
        sscanf(line, "%*s\t%s\t%s\t%i", tAd, rAd, &packetSiz);
        //This checks to see is the broadcast address is the recieving MAC address
        if (strcmp(rAd, "ff:ff:ff:ff:ff:ff") == 0 || strcmp(rAd, "FF:FF:FF:FF:FF:FF") == 0 ) {
            //If the transmission was to the broadcast address then the line is skipped over and the loop starts from the next line
            continue;
        }
        //Checks to see if the transmission addresses are to be analysed
        else if (mode == 't') {
            //If its looking at transmission addresses then the string with the transmission address is broken up into the individual bytes and converted from a hexadecimal number to a decimal int to make comparisions easier, and the values are saved to the checkAd array to store the MAC address (In truth the hexadecimal values are converted to binary for the computer to understand but when called back they would only be in decimal not hexadecimal unless specified otherwise so that humans can understand the number)
            sscanf(tAd, "%2x:%2x:%2x:%2x:%2x:%2x", &checkAd[0], &checkAd[1], &checkAd[2], &checkAd[3], &checkAd[4], &checkAd[5]);
        }
        //Checks to see if the recieving addresses are to be analysed
        else if (mode == 'r') {
            //If its looking at recieving addresses then the string with the recieving address is broken up into the individual bytes and converted from a hexadecimal number to a decimal int to make comparisions easier, and the values are saved to the checkAd array to store the MAC address
            sscanf(rAd, "%2x:%2x:%2x:%2x:%2x:%2x", &checkAd[0], &checkAd[1], &checkAd[2], &checkAd[3], &checkAd[4], &checkAd[5]);
        }
        //Checks to see how many MAC addresses have already been added, if too many the program exits as a failure with an error message
        if (noAd > MAX_MAC) {
            printf("Number of unique MAC addresses is too many, limit is: %i\n", MAX_MAC);
            exit(EXIT_FAILURE);
        }
        //Checks to see if this is the first MAC address to be stored, if it is then it will be placed on the first position of the array to hold the collated MAC addresses
        else if (noAd == 0) {
            //This loop goes over each value for the MAC address and places it in the appropriate order for the address within the collated MAC address array
            for (int i = 0; i < MAC_AD_BYTES; i++) {
                macAdList[noAd][i] = checkAd[i];
            }
            //This adds the packet information onto the end of the array as the final piece to be added
            macAdList[noAd][MAC_AD_BYTES] = packetSiz;
            //This adds one to the number of MAC addresses on the collated array as something has now been added
            noAd++;
        }
        //If it's not the first MAC address to be added it has to be compared to the MAC addresses already on the collated array incase it has the same address
        else {
            //Loops over only the addresses in the array plus the first empty space to allow the programm to add to that empty space
            for (int i = 0; i <= noAd; i++) {
                //If the loop is over the empty space at the end of the filled array
                if (i == noAd) {
                    //The MAC addresses are added onto the end
                    for (int j = 0; j < MAC_AD_BYTES; j++) {
                        macAdList[noAd][j] = checkAd[j];
                    }
                    //And the packet information is added
                    macAdList[noAd][MAC_AD_BYTES] = packetSiz;
                    //Before updating the number of addresses stored in the array
                    noAd++;
                    //And exiting the loop to start with the next address
                    break;
                }
                //If there are addresses on the array and its not at the first empty space of the array the address is compaired to the address at the current position on the array
                else if (macAdList[i][0] == checkAd[0] && macAdList[i][1] == checkAd[1] && macAdList[i][2] == checkAd[2] && macAdList[i][3] == checkAd[3] && macAdList[i][4] == checkAd[4] && macAdList[i][5] == checkAd[5]) {
                    //If the address matches then the packet information is added to the existing packet information
                    macAdList[i][MAC_AD_BYTES] += packetSiz;
                    //And the loop is exited to start with the next address
                    break;
                }
            }
        }
    }
    //Once the packet inforamtion has been fully read through and proccessed, the stream is closed
    fclose(packets);
    //The contents of the collated array is printed to the output file through a loop
    for (int i = 0; i < noAd; i++) {
        //The MAC addresses are printed in hexadecimal format (%x) seperated by : characters, the numbers are 2 digits wide and any numbers that don't have 2 digits is prodeeded with a 0 (02), the MAC addresses are then seperated from the next field by a tab character (\t) and is followed by the packet information as a decimal int (%i) and the line is finished with a new line character (\n)
        fprintf(output, "%02x:%02x:%02x:%02x:%02x:%02x\t%i\n",macAdList[i][0], macAdList[i][1], macAdList[i][2], macAdList[i][3], macAdList[i][4], macAdList[i][5], macAdList[i][6]);
    }
    //Once the contents of the array have been printed the stream is closed
    fclose(output);
    //EXIT_SUCCESS (0) is returned to indicate successful running of the function
    return EXIT_SUCCESS;
}
/* This is the function to process the output file containing the collated MAC addresses and to compare the OUI with the address to match the vendor of the device that transmitted or recieved the data. The function takes in the name of the file cointaining the list of OUIs and vendor names  */
int analyseOUI (char OUIFileName[]) {
    //This is to hold the stream of the OUI file
    FILE *OUIFile;
    //This is to hold the stream of the output file
    FILE *output;
    //This is an array to read in each line of the file, the size is set to the size of the input buffer
    char line[BUFSIZ];
    //This is to hold the string of the OUI value
    char inOUI[OUI_CHAR_LEN];
    //This is hold the string of the name of the vendor for the corrosonding OUI
    char vendorName[MAX_VENDOR_CHAR];
    //This is to split the OUI into its individual bytes to compare them agains the MAC address
    int checkAd[OUI_BYTES];
    //This is to scan in the MAC address from the output file as a string
    char ad[OUI_CHAR_LEN];
    //This is an array to hold the proccessed MAC addresses and packet information, it is structured with it's length being the maximum number of MAC addresses the program is built to handle and the individual bytes of the MAC address for the length of the OUI for the first 3 values plus the packet information on the end
    int macAdList[MAX_MAC][OUI_BYTES + 1];
    //This is an int to hold the packet size data for the MAC address being looked at
    int packetSiz = 0;
    //This is an int holding the number of MAC addresses in the macAdList array, this is different to the number of MAC addresses that have been processed as this looks at unique MAC addresses
    int noAd = 0;
    //This is the value of the total packet data for the MAC addresses that don't match with a vendor from the OUI file, this is so that the UNKNOWN VENDOR listing can be added in at the end
    int unknownVendsPacSiz = 0;
    //This is a counter of the OUIs scanned in to make sure that they dont exceed the maximum number for the program
    int noOUI = 0;
    //This is a boolean value to check if the MAC address has been matched to a vendor and if it isn't then the packet data can be added to the UNKNOWN VENDOR total
    bool matched = false;
    //Opening the OUI file, supplying the file name and opening mode to fopen, in this case opening the file for reading only
    OUIFile = fopen(OUIFileName, "r");
    //Checking to see if the file has been opened successfully, if not it'll return a NULL pointer
    if (OUIFile == NULL) {
        //Print out the error information before terminating the process as a failure
        perror("Cannot open OUIfile");
        exit(EXIT_FAILURE);
    }
    output = fopen("output", "r+");
    //Checking to see if the file has been opened successfully, if not it'll return a NULL pointer
    if (output == NULL) {
        //Print out the error information before terminating the process as a failure
        perror("Cannot process output file");
        exit(EXIT_FAILURE);
    }
    //This is to read in each line from the packets file, it loops over each line scanning them into line and then processing the contents of line
    while (fgets(line, sizeof line, output) != NULL) {
        //This breaks down line into 2 parts, the first part is scanned in as a string into ad the second part is scanned into packetSiz as an integer and the two parts are seperated by a white space in this case \t
        sscanf(line, "%s\t%i", ad, &packetSiz);
        //Looking at the MAC address the address is broken up into the individual bytes and converted from a hexadecimal number to a decimal int to make comparisions easier, and the values are saved to the checkAd array to store the MAC address, only the first 3 bytes are saved as they are the only parts used in OUI identification and the last 3 byts are ignored because of the * flag
        sscanf(ad, "%2x:%2x:%2x:%*2x:%*2x:%*2x", &checkAd[0], &checkAd[1], &checkAd[2]);
        //Checks to see if this is the first MAC address to be stored, if it is then it will be placed in the first position of the array to hold the collated MAC addresses
        if (noAd == 0) {
            //This loop goes over each value for the MAC address and places it in the appropriate oder for the address within the collated MAC address array
            for (int i = 0; i < OUI_BYTES; i++) {
                macAdList[noAd][i] = checkAd[i];
            }
            //This adds the packet information onto the end of the array as the final piece to be added
            macAdList[noAd][OUI_BYTES] = packetSiz;
            //This adds one to the number of MAC addresses on the collated array as something has now been added
            noAd++;
        }
        //If its not on the first MAC address to be added it has to be compared to the MAC addresses already on the array incase it has the same address
        else {
            //Loops over only the addresses in the array plus the first empty space to allow the program to add to that empty space
            for (int i = 0; i <= noAd; i++) {
                //If the loop is over the empty space at the end of the filled array
                if (i == noAd) {
                    //The MAC addresses are added to the end of the file
                    for (int j = 0; j < OUI_BYTES; j++) {
                        macAdList[noAd][j] = checkAd[j];
                    }
                    //And the packet information is added
                    macAdList[noAd][OUI_BYTES] = packetSiz;
                    //Before updating the number of addresses store in the array
                    noAd++;
                    //And exiting the loop to start with the next address
                    break;
                    
                }
                //If there are addresses on the array and its not at the first empty space of the array the address is compared to the addresses at the current position on the array
                else if (macAdList[i][0] == checkAd[0] && macAdList[i][1] == checkAd[1] && macAdList[i][2] == checkAd[2]) {
                    //If the address matches then the packet information is added to the existing paket information
                    macAdList[i][OUI_BYTES] += packetSiz;
                    //And the loop is exited to start with the next address
                    break;
                }
            }
        }
    }
    //Once the information has been read from the input file the stream is closed
    fclose(output);
    //A new file titled output is overwritten over the old file to hold the new output information
    output = fopen("output", "w+");
    //Checking to see if the file has been successfully created, if not it'll return a NULL pointer
    if (output == NULL) {
        //Print out the error message before terminating the proccess as a failure
        perror("Cannot process output file");
        exit(EXIT_FAILURE);
    }
    //Loops over the MAC addresses cointained within the array
    for (int i = 0; i < noAd; i++) {
        //Sets a parameter of whether there has been a match with an OUI, initially set as false, used incase there is no match to add the packet data to UNKOWN VENDOR
        matched = false;
        //Loops over each line of the OUI vendor file to compare the MAC address with the OUI
        while (fgets(line, sizeof line, OUIFile) != NULL) {
            //After the OUI line has been scanned increase the OUI count
            noOUI++;
            //Checks to see if there are too many OUIs scanned in and terminates the program if there has been too many scanned in
            if (noOUI > MAX_OUI) {
                printf("Too many unique OUIs in file, maximum is: %i\n", MAX_OUI);
                exit(EXIT_FAILURE);
            }
            //This breaks down the line into 2 parts, the first part contains the OUI and this is scanned in as a string to inOUI. The second part contains the vendor name and this is also scanned in as a string, sscanf scans all characters including whitespace till it encounters a carriage return or newline character and saves this to vendorName
            sscanf(line, "%s\t%[^\r\n]", inOUI, vendorName);
            //Checks to make sure the vendor name doesn't execced the maximum number of characters otherwise it wont print correctly
            if (strlen(vendorName) > MAX_VENDOR_CHAR) {
                //Print an error message if the vendor name is too long and exit the program with a failure
                printf("Vendor name: %s too long, maximum limit %i char\n", vendorName, MAX_VENDOR_CHAR);
                exit(EXIT_FAILURE);
            }
            //Looking at the OUI this is broken up into the 3 bytes that represent the OUI protion of a MAC address, these values are converted from hexadecimal to a decimal int and stored on the array checkAd
            sscanf(inOUI, "%2x-%2x-%2x", &checkAd[0], &checkAd[1], &checkAd[2]);
            //Checks the OUI against the MAC address that the loop is up to in the array
            if (macAdList[i][0] == checkAd[0] && macAdList[i][1] == checkAd[1] && macAdList[i][2] == checkAd[2]) {
                //If the MAC address matches the OUI, its printed into the output file in the format MAC address, vendor name, packet information, seperated by \t and finished with \n. The decimal numbers of the MAC addresses are converted back to hexadecimal at 2 digits wide and a 0 proceeds any numbers that are only 1 digit long
                fprintf(output, "%02x:%02x:%02x\t%s\t%i\n", macAdList[i][0], macAdList[i][1], macAdList[i][2], vendorName, macAdList[i][3]);
                //Sets matched to true as a match has been made, and to prevent the packet information being added to UNKNOWN VENDOR
                matched = true;
                //Resets the stream position to the beginning of the OUI file
                rewind(OUIFile);
                //Resets the OUI counter to 0 so that it doesn't double up the counting
                noOUI = 0;
                //Exits the loop going through the OUI file and starts at the next MAC address in the array
                break;
            }
        }
        //If a match has been made after breaking out the OUI file read loop, the process ends up here and enters this statement to start on the next MAC address in the array
        if  (matched) {
            //This starts the next itteration of the loop
            continue;
        }
        //If there has been no match made then the packet information is added to the UNKNOWN VENDOR total
        else if (!matched) {
            //Adds the packets data to UNKNOWN VENDOR
            unknownVendsPacSiz += macAdList[i][3];
            //Resets the stream to the beginning of the OUI file
            rewind(OUIFile);
            //Resets the OUI counter to 0 so that it doesn't double up the counting
            noOUI = 0;
        }
    }
    //After all the MAC addresses have been match up with the OUIs, if there are any that counldn't be matched then unknownVendsPacSiz will not be 0 and the result will be printed, otherwise if all MAC addresses have been matched then this part is skipped
    if (unknownVendsPacSiz > 0) {
        //Print out the UNKNOWN VENDOR entry into the output file
        fprintf(output, "??:??:??\tUNKNOWN-VENDOR\t%i\n", unknownVendsPacSiz);
    }
    //Closes the OUI file stream
    fclose(OUIFile);
    //Closes the output file stream
    fclose(output);
    //Exit with EXIT_SUCCESS (0) to indicate success
    return EXIT_SUCCESS;
}
/* This is the main function where execution beings from */
int main(int argc, char *argv[]) {
    //Saves the second argument to check which method of analysing the packet file
    char mode = *argv[1];
    //If there are not enough arguments or too many arguments supplied the program prints the correct usage and terminates as a failure
    if(argc < 3 || argc > 4) {
        printf("Usage: ./wifistats-osx t|r packets [OUIfile]\n");
        exit(EXIT_FAILURE);
    }
    //Checks to see if the correct argument input has been supplied for analysing the packet file
    else if (mode == 't' || mode == 'r') {
        //Runs the function to analyse the packet information supplying the direction and the  the packet file to be used
        analyseMacAd(*argv[1], argv[2]);
        //Checks to see if an OUI file is supplied, if so then it analyses the file before calling sort
        if (argc == 4) {
            //Calls the function to analyse the OUI files supplying the OUI file to be used
            analyseOUI(argv[3]);
            //This is done before calling sort to sort the reults and output them, with the number of fields in the file to be sorted suppled to the function so it knows which fields to sort by
            sortoutput(3);
            exit(EXIT_SUCCESS);
        }
        //If no OUI file is suppled then the sort function is called on the MAC addresses
        else {
            //The sort is called with the number of fields so that the sort function knows which fields to sort by
            sortoutput(2);
            exit(EXIT_SUCCESS);
        }
    }
    //If mode is not a t or r then the usage information is printed and the program exits as a failure
    else {
        printf("Usage: ./wifistats-osx t|r packets [OUIfile]\n");
        exit(EXIT_FAILURE);
        }
    //If the process makes it down to this point something has gone wrong and the program should exit as a failure
    return EXIT_FAILURE;
}
