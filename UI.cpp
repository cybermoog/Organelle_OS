
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <locale.h>
#include <string.h>
#include <sys/stat.h>

#include "UI.h"

UI::UI(){
    numPatches = 0;
    numMenuEntries = 0;
    menuOffset = 9;
    cursorOffset = 1;
    patchIsRunning = 0;
    menuScreenTimeout = MENU_TIMEOUT;

    encoderEnabled = 1;     // encoder input is enabled

    newScreen = 0;
    currentScreen = MENU;

    menuScreen.clear();
    patchScreen.clear();
    auxScreen.clear();
}

int UI::checkFileExists (const char * filename){
    struct stat st;
    int result = stat(filename, &st);
    return result == 0;
}

void UI::encoderUp(void) {
    if (cursorOffset == 4) {
        if (!(menuOffset >= (numMenuEntries - 1))) menuOffset++;
    }
    if (!(cursorOffset >= 4)) cursorOffset++;
    
    selectedPatch = menuOffset + cursorOffset;
    drawPatchList();
}

void UI::encoderDown(void) {
    if (cursorOffset == 0) {
        if (!(menuOffset < 1)) menuOffset--;
    }
    if (!(cursorOffset < 1)) cursorOffset--;
    
    selectedPatch = menuOffset + cursorOffset;
    drawPatchList();
}

void UI::encoderPress(void){
    selectedPatch =  menuOffset + cursorOffset;
    printf("selected patch: %d, %s\n", selectedPatch, menuItems[selectedPatch]);
    
    // menu items 0-10 are part of system menu
    if (selectedPatch < 10) {    
        runSystemCommand();
    }
    else { 
        runPatch();       
    }
}

void UI::encoderRelease(void){

}

void UI::runSystemCommand(void){
    char cmd[256];
    if (!strcmp(menuItems[selectedPatch], "Reload")){
        printf("Reloading... ");
        sprintf(cmd, "/root/scripts/mount.sh");
        system(cmd);
        loadPatchList();
        drawPatchList();
    }
 
    else if (!strcmp(menuItems[selectedPatch], "Shutdown")){
        printf("Shutting down... ");
        sprintf(cmd, "/root/scripts/shutdown.sh &");
        system(cmd);
    }
    
    else if (!strcmp(menuItems[selectedPatch], "Info")){
        printf("Displaying system info... ");

        auxScreen.clear();
        auxScreen.drawNotification("     System Info     ");

        auxScreen.setLine(1, "Software Version: 1");
        auxScreen.setLine(2, "USB: ...");
        auxScreen.setLine(3, "MIDI: ...");
        auxScreen.setLine(4, "CPU Load: ...");
        
        newScreen = 1;
        currentScreen = AUX;
    }
     
    else if (!strcmp(menuItems[selectedPatch], "Eject")){
        printf("Ejecting USB drive... ");
        sprintf(cmd, "/root/scripts/eject.sh &");
        system(cmd);
    }
}

void UI::runPatch(void){
    char cmd[256];
    // check for X,
    // run pd with nogui if no X. also use smaller audio buf with nogui
    // the rest of the settings are in /root/.pdsettings
    if(system("/root/scripts/check-for-x.sh")){
        printf("starting in GUI mode");
        if (checkFileExists("/usbdrive/patches/mother.pd")) sprintf(cmd, "/usr/bin/pd -rt -audiobuf 10 /usbdrive/patches/mother.pd \"/usbdrive/patches/%s/main.pd\" &", menuItems[selectedPatch]);
        else sprintf(cmd, "/usr/bin/pd -rt -audiobuf 10 /root/mother.pd \"/usbdrive/patches/%s/main.pd\" &", menuItems[selectedPatch]);
    }
    else {
        printf("starting in NON GUI mode");
        if (checkFileExists("/usbdrive/patches/mother.pd")) sprintf(cmd, "/usr/bin/pd -rt -nogui -audiobuf 4 /usbdrive/patches/mother.pd \"/usbdrive/patches/%s/main.pd\" &", menuItems[selectedPatch]);
        else sprintf(cmd, "/usr/bin/pd -rt -nogui -audiobuf 4 /root/mother.pd \"/usbdrive/patches/%s/main.pd\" &", menuItems[selectedPatch]);
    }

    // first kill any other PD
    system("/root/scripts/killpd.sh");
    system(cmd);
    patchIsRunning = 1;
    patchScreen.clear();
    currentScreen = PATCH;
    
    // put the patch name on the menu screen
    sprintf(cmd, "> %s", menuItems[selectedPatch]);
    menuScreen.drawNotification(cmd);
}

void UI::drawPatchList(void){
    char line[256];
    int i;
    for (i=0; i<5; i++) {
        sprintf(line, "%s", menuItems[i + menuOffset]);
        menuScreen.setLine(i + 1, line);
    }
 
    // dont invert patch lines if there are no patches
    if ((selectedPatch >= 10) && !numPatches) {
    }
    else {
        menuScreen.invertLine(cursorOffset);   
    }

    if (!patchIsRunning) {
        menuScreen.drawNotification("Select a patch...");
    }

    newScreen = 1;
    menuScreenTimeout = MENU_TIMEOUT;
    currentScreen = MENU;
    //printf("c %d, p %d\n", cursorOffset, menuOffset);
}

void UI::loadPatchList(void){

    char cmd[256];

    // find patches
    struct dirent **namelist;
    int n;
    int i;
  
    // clear em out
    for (i = 0; i < 127; i++){
        strcpy(menuItems[i], "");
    }

    // inititial patches
    numMenuEntries = 0;
    strcpy(menuItems[0], "");
    numMenuEntries++;
    strcpy(menuItems[1], "");
    numMenuEntries++;
    strcpy(menuItems[2], "------ SYSTEM -------");
    numMenuEntries++;

    strcpy(menuItems[3], "Eject");
    numMenuEntries++;

    strcpy(menuItems[4], "Reload");
    numMenuEntries++;

    strcpy(menuItems[5], "Info");
    numMenuEntries++;

    strcpy(menuItems[6], "Shutdown");
    numMenuEntries++;

    strcpy(menuItems[7], "");
    numMenuEntries++;
    strcpy(menuItems[8], "");
    numMenuEntries++;
    strcpy(menuItems[9], "------ PATCHES ------");
    numMenuEntries++;


    // set locale so sorting happens in right order
    setlocale(LC_ALL, "en_US.UTF-8");

    numPatches = 0;
    n = scandir("/usbdrive/patches", &namelist, NULL, alphasort);
    if (n<0)
        perror("scandir");
    else {

       while (n--) {
            if (namelist[n]->d_type == DT_DIR && strcmp (namelist[n]->d_name, "..") != 0 && strcmp (namelist[n]->d_name, ".") != 0) {
                strcpy(menuItems[numMenuEntries], namelist[n]->d_name);
                numMenuEntries++;
                numPatches++;
                numMenuEntries &= 0x7f;  // 128 max num patches
            }
            free(namelist[n]);
        }
        free(namelist);
    }
    for (i=0; i<numMenuEntries; i++) {
        printf("patch[%d]: %s\n", i, menuItems[i]);
    }

    // notify if no patches found
    if (!numPatches){
        strcpy(menuItems[10], "No patches found!");
        strcpy(menuItems[11], "Insert USB drive ");
        strcpy(menuItems[12], "with 'Patches' Folder.");
        strcpy(menuItems[13], "Then select Reload.");
    }

    // set cursor to beg
    menuOffset = 9;
    cursorOffset = 1;

    // kill pd 
    printf("stopping pd... \n");
    sprintf(cmd, "/root/scripts/killpd.sh ");
    system(cmd);
    patchIsRunning = 0;

}



