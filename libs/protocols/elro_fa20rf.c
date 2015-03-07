/*
	Copyright (C) 2015 CurlyMo and Meloen

	This file is part of pilight.

	pilight is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later
	version.

	pilight is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "elro_fa20rf.h"

static void elro_fa20rfCreateMessage(int unitcode, int state) {
	elro_fa20rf->message = json_mkobject();
	json_append_member(elro_fa20rf->message, "unitcode", json_mknumber(unitcode, 0));
	if(state == 0) {
		json_append_member(elro_fa20rf->message, "state", json_mkstring("opened"));
	} else {
		json_append_member(elro_fa20rf->message, "state", json_mkstring("closed"));
	}
}

static void elro_fa20rfParseBinary(void) {

	/*int i = 0, x = 0;
	int state = -1, unitcode = -1;
	*/
	/*
	for(i=0;i<elro_fa20rf->rawlen;i+=2) {
		elro_fa20rf->binary[x++] = elro_fa20rf->code[i];
	}
	*/
	
	/*
	for(x=0; x<elro_fa20rf->rawlen; x+=2) {
		if(elro_fa20rf->code[x+1] == 1) { 
			elro_fa20rf->binary[x/2]=1; 
 		} else { 
			elro_fa20rf->binary[x/2]=0; 
 		}
 	}
	
	unitcode = binToDecRev(elro_fa20rf->binary, 2, 22);
	state = binToDecRev(elro_fa20rf->binary[23]);
	*/
	
	/*int unitcode = binToDec(elro_fa20rf->binary, 0, 23);
	int state = elro_fa20rf->binary[24];
	*/
int x = 0; 

 
 	/* Convert the one's and zero's into binary */ 
 	for(x=0; x<elro_fa20rf->rawlen; x+=2) { 
 		if(elro_fa20rf->code[x+1] == 1) { 
 			elro_fa20rf->binary[x/2]=1; 
 		} else { 
 			elro_fa20rf->binary[x/2]=0; 
 		} 
 	} 
 
 
 	int unitcode = binToDec(elro_fa20rf->binary, 0, 23); 
 	int state = elro_fa20rf->binary[24]; 	
	elro_fa20rfCreateMessage(unitcode, state);
}

static void elro_fa20rfCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		elro_fa20rf->raw[i]=(2*elro_fa20rf->plslen->length);
		elro_fa20rf->raw[i+1]=(2*elro_fa20rf->pulse*elro_fa20rf->plslen->length);
	}
}

static void elro_fa20rfCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		elro_fa20rf->raw[i]=(2*elro_fa20rf->plslen->length);
		elro_fa20rf->raw[i+1]=(7*elro_fa20rf->plslen->length);
	}
}

static void elro_fa20rfClearCode(void) {
	elro_fa20rfCreateLow(0,47);
	elro_fa20rfCreateHigh(48,49);
}

static void elro_fa20rfCreateHeader(void) {
	elro_fa20rf->raw[0]=(21*elro_fa20rf->plslen->length);
	elro_fa20rf->raw[1]=(2*elro_fa20rf->plslen->length);
}

static void elro_fa20rfCreateUnitCode(int unitcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(unitcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*2;
			elro_fa20rfCreateHigh(2+x, 2+x+1);
		}
	}
}

static void elro_fa20rfCreateState(int state) {
	if(state == 1) {
		elro_fa20rf->raw[48]=(2*elro_fa20rf->plslen->length);
		elro_fa20rf->raw[49]=(4*elro_fa20rf->plslen->length);
	} 
}

static void elro_fa20rfCreateFooter(void) {
	elro_fa20rf->raw[50]=(2*elro_fa20rf->plslen->length);
	elro_fa20rf->raw[51]=(34*elro_fa20rf->plslen->length);
}

static int elro_fa20rfCreateCode(JsonNode *code) {
	int unitcode = -1;
	int state = -1;
	double itmp = 0;

	if(json_find_number(code, "unitcode", &itmp) == 0)
		unitcode = (int)round(itmp);
	if(json_find_number(code, "closed", &itmp) == 0)
		state=1;
	else if(json_find_number(code, "opened", &itmp) == 0)
		state=0;

	if(unitcode == -1 || state == -1) {
		logprintf(LOG_ERR, "elro_fa20rf: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(unitcode > 99999999 || unitcode < 0) {
		logprintf(LOG_ERR, "elro_fa20rf: invalid unitcode range");
		return EXIT_FAILURE;
	} else {
		elro_fa20rfCreateMessage(unitcode, state);
		elro_fa20rfClearCode();
		elro_fa20rfCreateHeader();
		elro_fa20rfCreateUnitCode(unitcode);
		elro_fa20rfCreateState(state);
		elro_fa20rfCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void elro_fa20rfPrintHelp(void) {
	printf("\t -u --unitcode=unitcode\t\tcontrol a device with this unitcode\n");
	printf("\t -t --opened\t\t\tsend an opened signal\n");
	printf("\t -f --closed\t\t\tsend an closed signal\n");
}

#if defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif

void elro_fa20rfInit(void) {

	protocol_register(&elro_fa20rf);
	protocol_set_id(elro_fa20rf, "elro_fa20rf");
	protocol_device_add(elro_fa20rf, "elro_fa20rf", "elro_fa20rf smoke detector");
	protocol_plslen_add(elro_fa20rf, 393);
	elro_fa20rf->devtype = CONTACT;
	elro_fa20rf->hwtype = RF433;
	elro_fa20rf->pulse = 2;
	elro_fa20rf->rawlen = 52;
	elro_fa20rf->binlen = 12;
	
	options_add(&elro_fa20rf->options, 'u', "unitcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&elro_fa20rf->options, 't', "opened", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&elro_fa20rf->options, 'f', "closed", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&elro_fa20rf->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	elro_fa20rf->parseBinary=&elro_fa20rfParseBinary;
	elro_fa20rf->createCode=&elro_fa20rfCreateCode;
	elro_fa20rf->printHelp=&elro_fa20rfPrintHelp;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "elro_fa20rf";
	module->version = "0.2";
	module->reqversion = "6.0";
	module->reqcommit = "187";
}

void init(void) {
	elro_fa20rfInit();
}
#endif
