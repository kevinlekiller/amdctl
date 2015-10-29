/**
 * Copyright 2015  kevinlekiller
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

void printBaseFmt();
void printPstates();
int getDec(const char *);
void getReg(const uint32_t);
void setReg(const uint32_t, const char *, int);
void getVidType();
float vidTomV(const int);
int mVToVid(float);
void defineFamily();
void getCpuInfo();
void error(const char *);
void test();

#define PSTATE_CURRENT_LIMIT 0xc0010061
#define PSTATE_CONTROL       0xc0010062
#define PSTATE_STATUS        0xc0010063
#define PSTATE_BASE          0xc0010064
#define COFVID_STATUS        0xc0010071

#define AMD10H 0x10 // K10
#define AMD11H 0x11 // puma
#define AMD12H 0x12 // llanna/fusion
#define AMD13H 0x13 // unknown
#define AMD14H 0x14 // bobcat
#define AMD15H 0x15 // bulldozer/piledriver/steamroller/excavator/etc
#define AMD16H 0x16 // godvari/kaveri/kabini/jaguar/beema/etc
#define AMD17H 0x17 // zen

#define PSTATE_MAX_VAL_BITS          "6:4"
#define CUR_PSTATE_LIMIT_BITS        "2:0"
#define PSTATE_CMD_BITS              "2:0"
#define CUR_PSTATE_BITS              "2:0"
#define IDD_DIV_BITS                 "41:40"
#define IDD_VALUE_BITS               "39:32"
#define CPU_VID_BITS                 "15:9"
#define COFVID_CUR_PSTATE_LIMIT_BITS "58:56"
#define MAX_CPU_COF_BITS             "54:49"
#define MIN_VID_BITS                 "48:42"
#define MAX_VID_BITS                 "41:35"

static char *NB_VID_BITS  = "31:25";
static char *CPU_DID_BITS = "8:6";
static char *CPU_FID_BITS = "5:0";

static int PSTATES = 8;
static uint64_t buffer;
static int core;
static int pvi = 0; // Seems like only some 10h use pvi?
static int cpuFamily = 0;
static int cpuModel = 0;
static int minMaxVid = 1;

void defineFamily() {
	switch (cpuFamily) {
	case AMD10H:
		getVidType();
		PSTATES = 5;
		break;
	case AMD12H:
		CPU_DID_BITS = "8:4";
		CPU_FID_BITS = "3:0";
		break;
	case AMD11H:
	case AMD13H:
		break;
	case AMD15H:
		if (cpuModel > 0x0f) {
			minMaxVid = 0;
			NB_VID_BITS = "31:24";
		}
		break;
	case AMD16H:
		minMaxVid = 0;
		NB_VID_BITS = "31:24";
		break;
	case AMD14H: // Disabled due to differences in cpu vid / did / fid
	case AMD17H: // Disabled because no BKDG currently.
	default:
		error("Your CPU family is unsupported.");
	}
}

// TODO parse args
int main(const int argc, const char *argv[]) {
	getCpuInfo();
	defineFamily();

//test();

	printf("Voltage ID encodings: %s\n", (pvi ? "PVI (parallel)" : "SVI (serial)"));
	int cores = 1;
	for (core = 0; core < cores; core++) {
		printf("CPU Core %d\n", core);
		printPstates();
	}
	return EXIT_SUCCESS;
}

void getCpuInfo() {
	FILE *fp;
	char buff[8192];
	
	fp = fopen("/proc/cpuinfo", "r");
	if (fp == NULL) {
		error("Could not open /proc/cpuinfo for reading.");
	}

	while(fgets(buff, 8192, fp)) {
		if (strstr(buff, "vendor_id") != NULL && strstr(buff, "AMD") == NULL) {
			error("Processor is not an AMD?");
		} else if (strstr(buff, "cpu family") != NULL) {
			sscanf(buff, "%*s %*s : %d", &cpuFamily);
		} else if (strstr(buff, "model") != NULL) {
			sscanf(buff, "%*s : %d", &cpuModel);
		}
		if (cpuFamily && cpuModel) {
			break;
		}
	}
	
	fclose(fp);
	if (!cpuModel || !cpuFamily) {
		error("Could not find CPU family or model!");
	}
	printf("Detected cpu model %xh, from family %xh\n", cpuModel, cpuFamily);
}

void usage() {
	printf("WARNING: This software can damage your CPU, use with caution.\n");
	printf("amdctl [options]\n");
	printf("    -g  --get      Get P-state information.\n");
	printf("    -c  --cpu      CPU core to work on. (all cores if not set)\n");
	printf("    -p  --pstate   P-state to work on. (all P-states if not set)\n");
	printf("    -v  --setcv    Set cpu voltage for P-state(millivolts, 1.4volts=1400 millivolts).\n");
	printf("    -n  --setnv    Set north bridge voltage (millivolts).\n");
	printf("    -t  --test     Show changes without applying them to the CPU.\n");
	printf("    -h  --help     Shows this informations.\n");
}

// Test setting voltage to 1450mV on P-State 0 of core 0
void test() {
	core = 0; // Sets CPU core to 0.
	getReg(PSTATE_BASE);  // Gets the info for P-State 0
	printBaseFmt(); // Prints the info.
	int oldVolt = vidTomV(getDec(CPU_VID_BITS)); // Get voltage of p-state.
	setReg(PSTATE_BASE, CPU_VID_BITS, mVToVid((oldVolt - 25))); // Reduces voltage for CPU 0 P-State 0 by 0.025v
	getReg(PSTATE_BASE); // Get info for P-State 0
	printBaseFmt(); // Print infor for P-State 0.
	setReg(PSTATE_BASE, CPU_VID_BITS, mVToVid(oldVolt)); // Reset voltage to normal.
	getReg(PSTATE_BASE); // Get info for P-State 0
	printBaseFmt(); // Print infor for P-State 0.
	exit(EXIT_SUCCESS);
}

void printPstates() {
	int pstate;
	uint32_t reg = (PSTATE_BASE - 1);
	for (pstate = 0; pstate < PSTATES; pstate++) {
		printf("\tP-State %d\n", pstate);
		reg += 1;
		getReg(reg);
		printBaseFmt();
	}

	printf("\tCOFVID Status P-State\n");
	getReg(COFVID_STATUS);
	printBaseFmt();
}

void printBaseFmt() {
	int CpuVid = getDec(CPU_VID_BITS);
	int CpuDid = getDec(CPU_DID_BITS);
	int CpuFid = getDec(CPU_FID_BITS);
	float CpuVolt = vidTomV(CpuVid);
	int NbVid  = getDec(NB_VID_BITS);
	float NbVolt = vidTomV(NbVid);
	int CpuMult = ((CpuFid + 0x10) / (2 ^ CpuDid));
	float CpuFreq = ((100 * (CpuFid + 0x10)) >> CpuDid);

	printf("\t\tCPU voltage id          %d\n", CpuVid);
	printf("\t\tCPU divisor id          %d\n", CpuDid);
	printf("\t\tCPU frequency id        %d\n", CpuFid);
	printf("\t\tCPU multiplier          %dx\n", CpuMult);
	printf("\t\tCPU frequency           %.2fMHz\n", CpuFreq);
	printf("\t\tCPU voltgage            %.2fmV\n", CpuVolt);
	printf("\t\tNorth Bridge voltage id %d\n", NbVid);
	printf("\t\tNorth Bridge voltage    %.2fmV\n", NbVolt);
}

void getReg(const uint32_t reg) {
	char path[32];
	int fh;
	uint64_t tmp_buffer;

	sprintf(path, "/dev/cpu/%d/msr", core);
	fh = open(path, O_RDONLY);
	if (fh < 0) {
		error("Could not open CPU for reading!");
	}

	if (pread(fh, &tmp_buffer, 8, reg) != sizeof buffer) {
		close(fh);
		error("Could not get data from CPU!");
	}
	close(fh);
	buffer = tmp_buffer;
}

void setReg(const uint32_t reg, const char *loc, int replacement) {
	int low;
	int high;
	uint64_t temp_buffer = buffer;
	char path[32];
	int fh;

	sscanf(loc, "%d:%d", &high, &low);
	if (low > high) {
		int temp = low;
		low = high;
		high = temp;
	}

	temp_buffer = ((temp_buffer & (~(high << low))) | (replacement << low));

	sprintf(path, "/dev/cpu/%d/msr", core);
	fh = open(path, O_WRONLY);
	if (fh < 0) {
		error("Could not open CPU for writing!");
	}

	if (pwrite(fh, &temp_buffer, sizeof temp_buffer, reg) != sizeof temp_buffer) {
		close(fh);
		error("Could not change value!");
	}
	close(fh);
}

int getDec(const char *loc) {
	int high;
	int low;
	int bits;
	uint64_t temp = buffer;

	// From msr-tools.
	sscanf(loc, "%d:%d", &high, &low);
	bits = high - low + 1;
	if (bits < 64) {
		temp >>= low;
		temp &= (1ULL << bits) - 1;
	}
	if (temp & (1ULL << (bits - 1))) {
		temp &= ~(1ULL << (temp - 1));
		temp = -temp;
	}
	return (int)temp;
}

// Ported from k10ctl
float vidTomV(const int vid) {
	if (pvi) {
		if (vid < 32) {
			return (1550 - vid * 25);
		}
		return (1162.5 - (vid > 63 ? 63 : vid) * 12.5);
	}
	return (1550 - (vid > 124 ? 124 : vid) * 12.5);
}

int mVToVid(float mV) {
	int maxVid = 124;
	int i;
	float tmpv;
	float volt = 1550;
	float mult = 12.5;
	if (mV > 1550 || mV < 1) {
		// TODO CHECK ERROR IN ARGV
		return 0;
	}
	if (pvi) {
		if (mV > 1162.5) {
			mult = 25;
		} else {
			maxVid = 63;
			volt = 1162.5;
		}
	}
	float min = (mV - (mult / 2));
	float max = (mV + (mult / 2));
	for (i = 1; i <= maxVid; i++) {
		tmpv = volt - i * mult;
		if (tmpv >= min && tmpv <= max) {
			printf("Found vid %d for voltage %.2f\n", i, tmpv);
			return i;
		}
	}
	return 0;
}

// Ported from k10ctl
void getVidType() {
	int fh;
	char buff[256];

	fh = open("/proc/bus/pci/00/18.3", O_RDONLY);
	if (fh < 0) {
		error("Unsupported CPU? Could not open /proc/bus/pci/00/18.3 ; Requires root permissions.");
	}

	if (read(fh, &buff, 256) != 256) {
		close(fh);
		error("Unsupported CPU? Could not read data from /proc/bus/pci/00/18.3");
	}
	close(fh);

	if (buff[3] != 0x12 || buff[2] != 0x3 || buff[1] != 0x10 || buff[0] != 0x22) {
		error("Unsupported CPU? Could not find voltage encodings.");
	}
	pvi = ((buff[0xa1] & 1) == 1);
}


void error(const char *message) {
	fprintf(stderr, "ERROR: %s\n", message);
	exit(EXIT_FAILURE);
}
