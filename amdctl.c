/**
 * Copyright (C) 2015  kevinlekiller
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
#include <getopt.h>

void printBaseFmt(const int idd);
int getDec(const char *);
void getReg(const uint32_t);
void setReg(const uint32_t, const char *, int);
void getVidType();
float vidTomV(const int);
int mVToVid(float);
void calculateDidFid(int, int *, int *);
void getCpuInfo();
void checkFamily();
void error(const char *);
void usage();

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

static uint64_t buffer;
static int PSTATES = 8, cpuFamily = 0, cpuModel = 0, cores = 0, pvi = 0, debug = 0,
minMaxVid = 0, testMode = 0, core = -1, pstate = -1, writeReg = 1;

int main(int argc, char **argv) {
	getCpuInfo();
	checkFamily();
	
	int low = -1, high = -1, nv = 0, cv = 0, c, opts = 0, cpuSpeed = 0;
	while ((c = getopt(argc, argv, "gdhtc:l:m:n:p:v:s:")) != -1) {
		opts = 1;
		switch (c) {
			case 'g':
				break;
			case 't':
				testMode = 1;
				break;
			case 'd':
				debug = 1;
				break;
			case 'c':
				core = atoi(optarg);
				if (core > cores || core < 0) {
					error("Option -c must be lower or equal to total number of CPU cores.");
				}
				break;
			case 'l':
				low = atoi(optarg);
				if (low < 0 || low >= PSTATES) {
					error("Option -l must be less than total number of P-States (8 or 5 depending on CPU).");
				}
				break;
			case 'm':
				high = atoi(optarg);
				if (high < 0 || high >= PSTATES) {
					error("Option -m must be less than total number of P-States (8 or 5 depending on CPU).");
				}
				break;
			case 'n':
				nv = atoi(optarg);
				if (nv < 1 || nv > 1550) {
					error("Option -n must be between 1 and 1550.");
				}
				break;
			case 'p':
				pstate = atoi(optarg);
				if (pstate > -1 && pstate < PSTATES) {
					break;
				} else {
					error("Option -p must be less than total number of P-States (8 or 5 depending on CPU).");
				}
			case 's':
				cpuSpeed = atoi(optarg);
				if (cpuSpeed < 100 || cpuSpeed > 6300) {
					error("Option -s must be between 100 and 6300.");
				}
				break;
			case 'v':
				cv = atoi(optarg);
				if (cv < 1 || cv > 1550) {
					error("Option -v must be between 1 and 1550.");
				}
				break;
			case '?':
			case 'h':
			default:
				usage();
		}
	}
	
	if (!opts) {
		usage();
	}

	printf("Voltage ID encodings: %s\n", (pvi ? "PVI (parallel)" : "SVI (serial)"));
	printf("Detected CPU model %xh, from family %xh with %d CPU cores.\n", cpuModel, cpuFamily, cores);
	if (nv || cv || low > -1 || high > -1) {
		printf("%s\n", (testMode ? "Preview mode On - No P-State values will be changed." : "PREVIEW MODE OFF - P-STATES WILL BE CHANGED!"));
	}
	
	if (core == -1) {
		core = 0;
	} else {
		cores = core + 1;
	}

	uint32_t tmp_pstates[PSTATES];
	int pstates_count = 0;
	if (pstate == -1) {
		for (; pstates_count < PSTATES; pstates_count++) {
			tmp_pstates[pstates_count] = (PSTATE_BASE + pstates_count);
		}
	} else {
		tmp_pstates[0] = PSTATE_BASE + pstate;
		pstates_count = 1;
	}
	
	int fndCpuDid, fndCpuFid;
	if (cpuSpeed) {
		int *fndCpuDidp = &fndCpuDid, *fndCpuFidp = &fndCpuFid;
		calculateDidFid(cpuSpeed, fndCpuDidp, fndCpuFidp);
	}

	for (; core < cores; core++) {
		printf("CPU Core %d\n", core);
		int i;
		for (i = 0; i < pstates_count; i++) {
			printf("\tP-State: %d\n", (pstate >= 0 ? pstate : i));
			getReg(tmp_pstates[i]);
			if (nv > 0) {
				setReg(tmp_pstates[i], NB_VID_BITS, mVToVid(nv));
			}
			if (cv > 0) {
				setReg(tmp_pstates[i], CPU_VID_BITS, mVToVid(cv));
			}
			if (cpuSpeed) {
				writeReg = 0;
				setReg(tmp_pstates[i], CPU_DID_BITS, fndCpuDid);
				writeReg = 1;
				setReg(tmp_pstates[i], CPU_FID_BITS, fndCpuFid);
			}
			getReg(tmp_pstates[i]); // Refresh for IDD values.
			printBaseFmt(1);
		}
		getReg(PSTATE_STATUS);
		printf("\tCurrent P-State: %d\n", getDec(CUR_PSTATE_BITS) + 1);
		getReg(COFVID_STATUS);
		printBaseFmt(0);
		if (low > -1) {
			setReg(PSTATE_CURRENT_LIMIT, PSTATE_MAX_VAL_BITS, low);
		}
		if (high > -1) {
			setReg(PSTATE_CURRENT_LIMIT, CUR_PSTATE_LIMIT_BITS, high);
		}
		getReg(PSTATE_CURRENT_LIMIT);
		puts("\tP-State Limits:");
		printf("\t\tMin: %d\n", getDec(PSTATE_MAX_VAL_BITS) + 1);
		printf("\t\tMax: %d\n", getDec(CUR_PSTATE_LIMIT_BITS) + 1);
	}

	return EXIT_SUCCESS;
}

void getCpuInfo() {
	if (debug) { printf("DEBUG: Checking CPU info.\n"); }
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
		} else if (strstr(buff, "cpu cores") != NULL) {
			sscanf(buff, "%*s %*s : %d", &cores);
		}
		if (cpuFamily && cpuModel && cores) {
			break;
		}
	}
	
	fclose(fp);
	if (!cpuModel || !cpuFamily || !cores) {
		error("Could not find CPU family or model!");
	}
}

void checkFamily() {
	if (debug) { printf("DEBUG: Setting variables based on CPU model.\n"); }
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
			error("Your CPU family is unsupported by amdctl.");
	}
}

void usage() {
	printf("WARNING: This software can damage your CPU, use with caution.\n");
	printf("amdctl [options]\n");
	printf("    -g    Get P-State information.\n");
	printf("    -c    CPU core to work on.\n");
	printf("    -p    P-state to work on.\n");
	printf("    -v    Set CPU voltage for P-state (millivolts).\n");
	printf("    -n    Set north bridge voltage (millivolts).\n");
	printf("    -l    Set the lowest useable (non turbo) P-State for the CPU core(s).\n");
	printf("    -m    Set the highest useable (non turbo) P-State for the CPU core(s).\n");
	printf("    -t    Preview changes without applying them to the CPU.\n");
	printf("    -s    Set the CPU frequency speed in MHz.\n");
	printf("    -d    Show debug info.\n");
	printf("    -h    Shows this information.\n");
	printf("Notes:\n");
	printf("    1 volt = 1000 millivolts.\n");
	printf("    All P-States are assumed if -p is not set.\n");
	printf("    All CPU cores assumed if -c is not set.\n");
	printf("Examples:\n");
	printf("    amdctl                      Shows this infortmation.\n");
	printf("    amdctl -g -c0               Displays all P-State info for CPU core 0.\n");
	printf("    amdctl -g -c3 -p1           Displays P-State 1 info for CPU core 3.\n");
	printf("    amdctl -v1400 -c2 -p0       Set CPU voltage to 1.4v on CPU core 2 P-State 0.\n");
	printf("    amdctl -v1350 -p1           Set CPU voltage to 1.35v for P-State 1 on all CPU cores.\n");
	printf("    amdctl -s3000 -v1300 -p1    Set CPU clock speed to 3ghz, CPU voltage to 1.3v for P-State on all CPU cores.\n");
	exit(EXIT_SUCCESS);
}

void printBaseFmt(const int idd) {
	const int CpuVid = getDec(CPU_VID_BITS);
	const int CpuDid = getDec(CPU_DID_BITS);
	const int CpuFid = getDec(CPU_FID_BITS);
	const int NbVid  = getDec(NB_VID_BITS);
	const float cpuVolt = vidTomV(CpuVid);

	printf("\t\tCPU voltage id          %d\n", CpuVid);
	printf("\t\tCPU divisor id          %d\n", CpuDid);
	printf("\t\tCPU frequency id        %d\n", CpuFid);
	printf("\t\tCPU multiplier          %dx\n", ((CpuFid + 0x10) / (2 ^ CpuDid)));
	printf("\t\tCPU frequency           %dMHz\n", ((100 * (CpuFid + 0x10)) >> CpuDid));
	printf("\t\tCPU voltgage            %.2fmV\n", cpuVolt);
	printf("\t\tNorth Bridge voltage id %d\n", NbVid);
	printf("\t\tNorth Bridge voltage    %.2fmV\n", vidTomV(NbVid));
	
	if (idd) {
		int IddDiv = getDec(IDD_DIV_BITS);
		printf("\t\tCPU current divisor id  %d\n", IddDiv);
		switch (IddDiv) {
			case 0b00:
				IddDiv = 1;
				break;
			case 0b01:
				IddDiv = 10;
				break;
			case 0b10:
				IddDiv = 100;
				break;
			case 0b11:
			default:
				return;
		}
		float cpuCurrent = (getDec(IDD_VALUE_BITS) / IddDiv);
		printf("\t\tCPU current draw       %2.fA\n", cpuCurrent);
		printf("\t\tCPU power draw         %2.fmW\n", (cpuCurrent * (cpuVolt / 1000)));
	}
}

void getReg(const uint32_t reg) {
	if (debug) { printf("DEBUG: Getting data from CPU %d at register %x\n", core, reg); }
	char path[32];
	int fh;

	sprintf(path, "/dev/cpu/%d/msr", core);
	fh = open(path, O_RDONLY);
	if (fh < 0) {
		error("Could not open CPU for reading! Is the msr kernel module loaded?");
	}

	if (pread(fh, &buffer, 8, reg) != sizeof buffer) {
		close(fh);
		error("Could not read data from CPU!");
	}
	close(fh);
}

void setReg(const uint32_t reg, const char *loc, int replacement) {
	if (debug) { printf("DEBUG: Setting data %d at register %x, location %s for CPU %d\n", replacement, reg, loc, core); }
	int low, high, fh;
	char path[32];

	sscanf(loc, "%d:%d", &high, &low);
	if (low > high) {
		int temp = low;
		low = high;
		high = temp;
	}

	buffer = ((buffer & (~(high << low))) | (replacement << low));

	if (!testMode && writeReg) {
		sprintf(path, "/dev/cpu/%d/msr", core);
		fh = open(path, O_WRONLY);
		if (fh < 0) {
			error("Could not open CPU for writing! Is the msr kernel module loaded?");
		}

		if (pwrite(fh, &buffer, sizeof buffer, reg) != sizeof buffer) {
			close(fh);
			error("Could write new value to CPU!");
		}
		close(fh);
	}
}

int getDec(const char *loc) {
	int high, low, bits;
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
			if (debug) { printf("Found vid %d for voltage %.2f\n", i, tmpv); }
			return i;
		}
	}
	return 0;
}

void calculateDidFid(int mhz, int *retDid, int *retFid) {
	int foundDid,i, tmp_mhz = 0;
	if (mhz >= 1600) {
		foundDid = 0x0;
	} else if (mhz >= 800) {
		foundDid = 0x1;
	} else if (mhz >= 400) {
		foundDid = 0x2;
	} else if (mhz >= 200) {
		foundDid = 0x3;
	} else {
		foundDid = 0x4;
	}
	for (i = 0; i <= 0x2f; i++) {
		tmp_mhz = ((100 * (i + 0x10)) >> foundDid);
		if (tmp_mhz >= mhz) {
			break;
		}
	}
	if (debug) { printf("Found did %d, fid %d (%dMHz) for wanted %dMHz\n", foundDid, i, tmp_mhz, mhz); }
	*retFid = i;
	*retDid = foundDid;
}

// Ported from k10ctl
void getVidType() {
	int fh;
	char buff[256];

	fh = open("/proc/bus/pci/00/18.3", O_RDONLY);
	if (fh < 0) {
		error("Unsupported CPU? Could not open /proc/bus/pci/00/18.3 ; Do you have the required permissions to read this file?");
	}

	if (read(fh, &buff, 256) != 256) {
		close(fh);
		error("Could not read data from /proc/bus/pci/00/18.3 ; Unsupported CPU?");
	}
	close(fh);

	if (buff[3] != 0x12 || buff[2] != 0x3 || buff[1] != 0x10 || buff[0] != 0x22) {
		error("Could not find voltage encodings from /proc/bus/pci/00/18.3 ; Unsupported CPU?");
	}
	pvi = ((buff[0xa1] & 1) == 1);
}


void error(const char *message) {
	fprintf(stderr, "ERROR: %s\n", message);
	exit(EXIT_FAILURE);
}
