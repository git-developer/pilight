/*
	Copyright (C) 2015 CurlyMo, Meloen and Scarala

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
#include "logilink.h"

static void logilinkCreateMessage(int systemcode, int unitcode, int state) {
	logilink->message = json_mkobject();
	json_append_member(logilink->message, "systemcode", json_mknumber(systemcode, 0));
	json_append_member(logilink->message, "unitcode", json_mknumber(unitcode, 0));
	if(state == 0) {
		json_append_member(logilink->message, "state", json_mkstring("on"));
	} else {
		json_append_member(logilink->message, "state", json_mkstring("off"));
	}
}

static void logilinkParseCode(void) {
	int i = 0, x = 0;
	int systemcode = 0, state = 0, unitcode = 0;

	for(i=0;i<logilink->rawlen-1;i+=2) {
		logilink->binary[x++] = logilink->code[i];
	}
	systemcode = binToDecRev(logilink->binary, 0, 19);
	state = logilink->binary[20];
	unitcode = binToDecRev(logilink->binary, 21, 23);

	logilinkCreateMessage(systemcode, unitcode, state);
}

static void logilinkCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		logilink->raw[i]=(logilink->plslen->length);
		logilink->raw[i+1]=(logilink->pulse*logilink->plslen->length);
	}
}

static void logilinkCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		logilink->raw[i]=(logilink->pulse*logilink->plslen->length);
		logilink->raw[i+1]=(logilink->plslen->length);
	}
}

static void logilinkClearCode(void) {
	logilinkCreateLow(0, logilink->rawlen-2);
}

static void logilinkCreateSystemCode(int systemcode) {
	int binary[1024];
	int length=0;
	int i=0, x=0;

	x=40;
	length = decToBin(systemcode, binary);
	for(i=length;i>=0;i--) {
		if(binary[i] == 0) {
			logilinkCreateHigh(x, x+1);
		}
		x=x-2;
	}
}

static void logilinkCreateUnitCode(int unitcode) {
		switch (unitcode) {
		case 7:
			logilinkCreateHigh(42, 47);	// Button 1
		break;
		case 3:
			logilinkCreateLow(42, 43); // Button 2
			logilinkCreateHigh(44, 47);	
		break;
		case 5:
			logilinkCreateHigh(42, 43); // Button 3
			logilinkCreateLow(44, 45);
			logilinkCreateHigh(46, 47);
		break;
		case 6:
			logilinkCreateHigh(42, 45); // Button 4
			logilinkCreateLow(46, 47);
		break;
		case 0:
			logilinkCreateLow(42, 47);	// Button ALL OFF
		break;
		default:
		break;
	}
}

static void logilinkCreateState(int state) {
	if(state == 1) {
		logilinkCreateLow(40, 41);
	} else {
		logilinkCreateHigh(40, 41);
	}
}

static void logilinkCreateFooter(void) {
	logilink->raw[48]=(logilink->plslen->length);
	logilink->raw[49]=(PULSE_DIV*logilink->plslen->length);
}

static int logilinkCreateCode(JsonNode *code) {
	int systemcode = -1;
	int unitcode = -1;
	int state = -1;
	double itmp = 0;

	if(json_find_number(code, "systemcode", &itmp) == 0)
		systemcode = (int)round(itmp);
	if(json_find_number(code, "unitcode", &itmp) == 0)
		unitcode = (int)round(itmp);
	if(json_find_number(code, "off", &itmp) == 0)
		state=1;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=0;

	if(systemcode == -1 || unitcode == -1 || state == -1) {
		logprintf(LOG_ERR, "logilink: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(systemcode > 2097151 || systemcode < 0) {
		logprintf(LOG_ERR, "logilink: invalid systemcode range");
		return EXIT_FAILURE;
	} else if(unitcode > 7 || unitcode < 0) {
		logprintf(LOG_ERR, "logilink: invalid unitcode range");
		return EXIT_FAILURE;
	} else {
		logilinkCreateMessage(systemcode, unitcode, state);
		logilinkClearCode();
		logilinkCreateSystemCode(systemcode);
		logilinkCreateUnitCode(unitcode);
		logilinkCreateState(state);
		logilinkCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void logilinkPrintHelp(void) {
	printf("\t -s --systemcode=systemcode\tcontrol a device with this systemcode\n");
	printf("\t -u --unitcode=unitcode\t\tcontrol a device with this unitcode\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void logilinkInit(void) {

	protocol_register(&logilink);
	protocol_set_id(logilink, "logilink");
	protocol_device_add(logilink, "logilink", "Logilink Switches");
	protocol_plslen_add(logilink, 284);
	logilink->devtype = SWITCH;
	logilink->hwtype = RF433;
	logilink->pulse = 3;
	logilink->rawlen = 50;
	logilink->binlen = 12;
	
	options_add(&logilink->options, 's', "systemcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&logilink->options, 'u', "unitcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&logilink->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&logilink->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);

	options_add(&logilink->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	logilink->parseCode=&logilinkParseCode;
	logilink->createCode=&logilinkCreateCode;
	logilink->printHelp=&logilinkPrintHelp;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "logilink";
	module->version = "0.1";
	module->reqversion = "6.0";
	module->reqcommit = "187";
}

void init(void) {
	logilinkInit();
}
#endif
