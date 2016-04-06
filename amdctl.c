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
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

void printBaseFmt(const int);
int getDec(const char *);
void getReg(const uint32_t);
void updateBuffer(const char *, const int);
void setReg(const uint32_t);
void getVidType();
double vidTomV(const int);
float getDiv(const int);
float getCpuMultiplier(const int, const int);
int getClockSpeed(const int, const int);
int mVToVid(const float);
void getCpuInfo();
void checkFamily();
void error(const char *);
void usage();
void fieldDescriptions();

#define PSTATE_CURRENT_LIMIT 0xc0010061
#define PSTATE_STATUS        0xc0010063
#define PSTATE_BASE          0xc0010064
#define COFVID_STATUS        0xc0010071

#define AMD10H 0x10 // K10
#define AMD11H 0x11 // Turion
#define AMD12H 0x12 // Fusion
#define AMD13H 0x13 // Unknown
#define AMD14H 0x14 // Bobcat
#define AMD15H 0x15 // Bulldozer
#define AMD16H 0x16 // Jaguar
#define AMD17H 0x17 // Zen

#define PSTATE_EN_BITS        "63:63"
#define PSTATE_MAX_VAL_BITS   "6:4"
#define CUR_PSTATE_LIMIT_BITS "2:0"
#define CUR_PSTATE_BITS       "2:0"
#define IDD_DIV_BITS          "41:40"
#define IDD_VALUE_BITS        "39:32"
#define CPU_VID_BITS          "15:9"

#define MAX_VOLTAGE  1550
#define MID_VOLTAGE  1162.5
#define MAX_VID      124
#define MID_VID      63
#define MIN_VID      32
#define VID_DIVIDOR1 25
#define VID_DIVIDOR2 12.5

static char *NB_VID_BITS  = "31:25";
static char *CPU_DID_BITS = "8:6";
static char *CPU_FID_BITS = "5:0";

static uint64_t buffer;
static int PSTATES = 8, DIDS = 5, cpuFamily = 0, cpuModel = 0, cores = 0,
		pvi = 0, debug = 0, quiet = 0, testMode = 0, core = -1, pstate = -1;

int main(int argc, char **argv) {
	getCpuInfo();
	checkFamily();

	int nv = -1, cv = -1, c, opts = 0, did = -1, fid = -1, currentOnly = 0, togglePs = -1, uVolt;
	while ((c = getopt(argc, argv, "eghistxa:c:d:f:l:m:n:p:u:v:")) != -1) {
		opts = 1;
		switch (c) {
			case 'a':
				togglePs = atoi(optarg);
				if (togglePs < 0 || togglePs > 1) {
					error("Option -a must be 1 or 0.");
				}
				break;
			case 'c':
				core = atoi(optarg);
				if (core > cores || core < 0) {
					error("Option -c must be lower or equal to total number of CPU cores.");
				}
				break;
			case 'd':
				did = atoi(optarg);
				if (did > DIDS || did < 0) {
					if (!quiet) {
						fprintf(stderr, "ERROR: Option -d must be a number 0 to %d\n", DIDS);
					}
					exit(EXIT_FAILURE);
				}
				break;
			case 'f':
				fid = atoi(optarg);
				if (fid > 0x2f || fid < 0) {
					error("Option -f must be a number 0 to 47");
				}
				break;
			case 'n':
				if (cpuFamily != AMD10H) {
					error("Currently amdctl can only change the NB vid on 10h CPU's.");
				}
				nv = atoi(optarg);
				if (nv < 0 || nv > 124) {
					error("Option -n must be between 0 and 124.");
				}
				break;
			case 'p':
				pstate = atoi(optarg);
				if (pstate > -1 && pstate < PSTATES) {
					break;
				} else {
					error("Option -p must be less than total number of P-States (8 or 5 depending on CPU).");
				}
				break;
			case 'u':
				uVolt = atoi(optarg);
				if (uVolt < 1 || uVolt > 1550) {
					error("Option -n must be between 1 and 1550.");
				}
				if (!quiet) {
					printf("Found vid %d for for %dmV.\n", mVToVid(uVolt), uVolt);
				}
				exit(EXIT_SUCCESS);
			case 'v':
				cv = atoi(optarg);
				if (cv < 0 || cv > 124) {
					error("Option -v must be between 0 and 124.");
				}
				break;
			case 'e':
				currentOnly = 1;
				break;
			case 'g':
				break;
			case 'i':
				debug = 1;
				break;
			case 's':
				quiet = 1;
				break;
			case 't':
				testMode = 1;
				break;
			case 'x':
				fieldDescriptions();
				break;
			case '?':
			case 'h':
			default:
				usage();
				break;
		}
	}

	if (!opts) {
		usage();
	}
	if (togglePs > -1 && pstate == -1) {
		error("You must pass the -p argument when passing the -x argument.");
	}

	if (!quiet) {
		printf("Voltage ID encodings: %s\n", (pvi ? "PVI (parallel)" : "SVI (serial)"));
		printf("Detected CPU model %xh, from family %xh with %d CPU cores.\n", cpuModel, cpuFamily, cores);
		if (nv > -1 || cv > -1 || fid > -1 || did > -1) {
			printf("%s\n", (testMode ? "Preview mode On - No P-State values will be changed."
									 : "PREVIEW MODE OFF - P-STATES WILL BE CHANGED!"));
		}
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

	for (; core < cores; core++) {
		getReg(PSTATE_CURRENT_LIMIT);
		int i, minPstate = getDec(PSTATE_MAX_VAL_BITS) + 1, maxPstate = getDec(CUR_PSTATE_LIMIT_BITS) + 1;
		getReg(PSTATE_STATUS);
		if (!quiet) {
			printf("\nCore %d | P-State Limits (non-turbo): Highest: %d ; Lowest %d | Current P-State: %d\n", core,
				   maxPstate, minPstate, getDec(CUR_PSTATE_BITS) + 1);
			if (cpuFamily == AMD10H) {
				printf(
						"%7s%7s%7s%7s%7s%8s%8s%8s%6s%7s%7s%7s%8s%9s\n",
						"Pstate", "Status", "CpuFid", "CpuDid", "CpuVid", "CpuMult", "CpuFreq", "CpuVolt", "NbVid",
						"NbVolt", "IddVal", "IddDiv", "CpuCurr", "CpuPower"
				);
			} else {
				printf(
						"%7s%7s%7s%7s%7s%8s%8s%8s%7s%7s%8s%9s\n",
						"Pstate", "Status", "CpuFid", "CpuDid", "CpuVid", "CpuMult", "CpuFreq", "CpuVolt",
						"IddVal", "IddDiv", "CpuCurr", "CpuPower"
				);
			}
		}
		if (!currentOnly) {
			for (i = 0; i < pstates_count; i++) {
				if (!quiet) {
					printf("%7d", (pstate >= 0 ? pstate : i));
				}
				getReg(tmp_pstates[i]);
				if (nv > -1 || cv > -1 || fid > -1 || did > -1 || togglePs > -1) {
					if (togglePs > -1) {
						updateBuffer(PSTATE_EN_BITS, togglePs);
					}
					if (nv > -1) {
						updateBuffer(NB_VID_BITS, nv);
					}
					if (cv > -1) {
						updateBuffer(CPU_VID_BITS, cv);
					}
					if (fid > -1) {
						updateBuffer(CPU_FID_BITS, fid);
					}
					if (did > -1) {
						updateBuffer(CPU_DID_BITS, did);
					}
					setReg(tmp_pstates[i]);
				}
				printBaseFmt(1);
				if (i >= minPstate) {
					break;
				}
			}
		}
		if (!quiet) {
			printf("%7s", "current");
		}
		getReg(COFVID_STATUS);
		printBaseFmt(0);
	}

	return EXIT_SUCCESS;
}

void getCpuInfo() {
	if (debug && !quiet) { printf("DEBUG: Checking CPU info.\n"); }
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
		} else if (strstr(buff, "siblings") != NULL) {
			sscanf(buff, "%*s : %d", &cores);
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
	if (debug && !quiet) { printf("DEBUG: Setting variables based on CPU model.\n"); }
	switch (cpuFamily) {
		case AMD10H:
			getVidType();
			PSTATES = 5;
			break;
		case AMD11H:
			DIDS = 4;
			break;
		case AMD12H:
			DIDS = 8;
			CPU_DID_BITS = "3:0";
			CPU_FID_BITS = "8:4";
			break;
		case AMD15H:
			if (cpuModel > 0x0f) {
				NB_VID_BITS = "31:24";
			}
			break;
		case AMD16H:
			NB_VID_BITS = "31:24";
			break;
		case AMD13H:
		case AMD14H: // Disabled due to differences in cpu vid / did / fid
		case AMD17H: // Disabled because no BKDG currently.
		default:
			error("Your CPU family is unsupported by amdctl.");
	}
}

void usage() {
	printf("WARNING: This software can damage your CPU, use with caution.\n");
	printf("Tool for under/over clocking/volting AMD CPU's.\n");
	printf("Supported AMD CPU families: 10h,11h,12h,15h,16h\n");
	printf("These AMD CPU families are unsupported: 13h,14h,17h, anything older than 10h or newer than 17h\n");
	printf("Usage:\n");
	printf("amdctl [options]\n");
	printf("    -g    Get P-State information.\n");
	printf("    -c    CPU core to work on.\n");
	printf("    -p    P-state to work on.\n");
	printf("    -v    Set CPU voltage id (vid).\n");
	if (cpuFamily == AMD10H) {
		printf("    -n    Set north bridge voltage id (vid).\n");
	}
	printf("    -d    Set the CPU divisor id (did).\n");
	printf("    -f    Set the CPU frequency id (fid).\n");
	printf("    -a    Activate (1) or deactivate (0) P-state.\n");
	printf("    -e    Show current P-State only.\n");
	printf("    -t    Preview changes without applying them to the CPU.\n");
	printf("    -u    Try to find voltage id by voltage (millivolts).\n");
	printf("    -s    Hide all output / errors.\n");
	printf("    -i    Show debug info.\n");
	printf("    -h    Shows this information.\n");
	printf("    -x    Explains field name descriptions.\n");
	printf("Notes:\n");
	printf("    1 volt = 1000 millivolts.\n");
	printf("    All P-States are assumed if -p is not set.\n");
	printf("    All CPU cores assumed if -c is not set.\n");
	printf("Examples:\n");
	printf("    amdctl                      Shows this infortmation.\n");
	printf("    amdctl -g -c0               Displays all P-State info for CPU core 0.\n");
	printf("    amdctl -g -c3 -p1           Displays P-State 1 info for CPU core 3.\n");
	exit(EXIT_SUCCESS);
}

void fieldDescriptions() {
	printf("Core:        Cpu core.\n");
	printf("P-State:     Power state, lower number means higher performance, 'current' means the P-State the CPU is in currently.\n");
	printf("Status:      If the P-State is enabled (1) or disabled (0).\n");
	printf("CpuFid:      Core frequency ID, with the CpuDid, this is used to calculate the core clock speed.\n");
	printf("CpuDid:      Core divisor ID, see CpuFid.\n");
	printf("CpuVid:      Core voltage ID, used to calculate the core voltage. (lower numbers mean higher voltage).\n");
	printf("CpuMult:     Core multiplier.\n");
	printf("CpuFreq:     Core clock speed, in megahertz.\n");
	printf("CpuVolt:     Core voltage, in millivolts.\n");
	printf("NbVid:       North bridge voltage ID.\n");
	printf("NbVolt:      North bridge voltage, in millivolts.\n");
	printf("IddVal:      Core current (intensity) ID. Used to calculate cpu current draw and power draw.\n");
	printf("IddDiv       Core current (intensity) dividor (IddVal / IddDiv = current draw).\n");
	printf("CpuCurr:     The cpu current draw, in amps.\n");
	printf("CpuPower:    The cpu power draw, in watts.\n");
	exit(EXIT_SUCCESS);
}

void printBaseFmt(const int idd) {
	const int CpuVid = getDec(CPU_VID_BITS), CpuDid = getDec(CPU_DID_BITS), CpuFid = getDec(CPU_FID_BITS);
	const int NbVid  = getDec(NB_VID_BITS), status = (idd ? getDec(PSTATE_EN_BITS) : 1);
	const double CpuVolt = vidTomV(CpuVid);
	if (!quiet) {
		if (cpuFamily == AMD10H) {
			printf(
					"%7d%7d%7d%7d%7.2fx%5dMHz%6.0fuV%6d%5.0fuV",
					status, CpuFid, CpuDid, CpuVid, getCpuMultiplier(CpuFid, CpuDid),
					getClockSpeed(CpuFid, CpuDid), CpuVolt, NbVid, vidTomV(NbVid)
			);
		} else {
			printf(
					"%7d%7d%7d%7d%7.2fx%5dMHz%6.0fuV",
					status, CpuFid, CpuDid, CpuVid, getCpuMultiplier(CpuFid, CpuDid),
					getClockSpeed(CpuFid, CpuDid), CpuVolt
			);
		}
	}
	if (idd) {
		int IddDiv = getDec(IDD_DIV_BITS), IddVal = getDec(IDD_VALUE_BITS);
		switch (IddDiv) {
			case 0:
				IddDiv = 1;
				break;
			case 1:
				IddDiv = 10;
				break;
			case 2:
				IddDiv = 100;
				break;
			case 3:
			default:
				if (!quiet) {
					printf("\n");
				}
				return;
		}
		float cpuCurrDraw = ((float)IddVal / IddDiv);
		if (!quiet) {
			printf("%7d%7d%7.2fA%8.2fW", IddVal, IddDiv, cpuCurrDraw, ((cpuCurrDraw * CpuVolt) / 1000));
		}
	}
	if (!quiet) {
		printf("\n");
	}
}

void getReg(const uint32_t reg) {
	if (debug && !quiet) { printf("DEBUG: Getting data from CPU %d at register %x\n", core, reg); }
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

float getCpuMultiplier(const int CpuFid, const int CpuDid) {
	float FidInc;
	switch (cpuFamily) {
		case AMD10H:
		case AMD15H:
		case AMD16H:
			FidInc = (CpuFid + 0x10);
			return (FidInc / (2 << CpuDid));
		case AMD11H:
			FidInc = (CpuFid + 0x08);
			return (FidInc / (2 << CpuDid));
		case AMD12H:
			FidInc = (CpuFid + 0x10);
			return (FidInc / getDiv(CpuDid));
		default:
			return 0;
	}
}

int getClockSpeed(const int CpuFid, const int CpuDid) {
	switch (cpuFamily) {
		case AMD10H:
		case AMD15H:
		case AMD16H:
			return ((100 * (CpuFid + 0x10)) >> CpuDid);
		case AMD11H:
			return ((100 * (CpuFid + 0x08)) >> CpuDid);
		case AMD12H:
			return (int) (100* (CpuFid + 0x10) / getDiv(CpuDid));
		default:
			return 0;
	}
}

float getDiv(const int CpuDid) {
	switch (cpuFamily) {
		case AMD11H:
			switch (CpuDid) {
				case 1:
					return 2;
				case 2:
					return 4;
				case 3:
					return 8;
				default:
					return 1;
			}
		case AMD12H:
			switch (CpuDid) {
				case 1:
					return 1.5;
				case 2:
					return 2;
				case 3:
					return 3;
				case 4:
					return 4;
				case 5:
					return 6;
				case 6:
					return 8;
				case 7:
					return 12;
				case 8:
					return 16;
				case 0:
				default:
					return 1;
			}
		default:
			switch (CpuDid) {
				case 1:
					return 2;
				case 2:
					return 4;
				case 3:
					return 8;
				case 4:
					return 16;
				default:
					return 1;
			}
	}
}

void setReg(const uint32_t reg) {
	if (!testMode) {
		int fh;
		char path[32];

		sprintf(path, "/dev/cpu/%d/msr", core);
		fh = open(path, O_WRONLY);
		if (fh < 0) {
			error("Could not open CPU for writing! Is the msr kernel module loaded?");
		}
		
		if (pwrite(fh, &buffer, sizeof buffer, reg) != sizeof buffer) {
			close(fh);
			error("Could not write new value to CPU!");
		}
		close(fh);
	}
}

void updateBuffer(const char *loc, const int replacement) {
	int low, high;

	sscanf(loc, "%d:%d", &high, &low);
	if (high == low) {
		if (replacement) {
			buffer |= (1ULL << high);
		} else {
			buffer &= ~(1ULL << high);
		}
	} else {
		if (replacement < (2 << (high - low))) {
			buffer = (buffer & ((1 << low) - (2 << high) - 1)) | (replacement << low);
		}
	}
}

int getDec(const char *loc) {
	uint64_t temp = buffer;
	int high, low, bits;

	// From msr-tools.
	sscanf(loc, "%d:%d", &high, &low);
	if (high == low) {
		return (int) ((temp >> high)  & 1ULL);
	} else {
		bits = high - low + 1;
		if (bits < 64) {
			temp >>= low;
			temp &= (1ULL << bits) - 1;
		}
		return (int) temp;
	}
}

// Ported from k10ctl
double vidTomV(const int vid) {
	if (pvi) {
		if (vid < MIN_VID) {
			return (MAX_VOLTAGE - vid * VID_DIVIDOR1);
		}
		return (MID_VOLTAGE - (vid > MID_VID ? MID_VID : vid) * VID_DIVIDOR2);
	}
	return (MAX_VOLTAGE - (vid > MAX_VID ? MAX_VID : vid) * VID_DIVIDOR2);
}

int mVToVid(const float mV) {
	int maxVid = MAX_VID, i;
	float tmpv, volt = MAX_VOLTAGE, mult = VID_DIVIDOR2, min, max;

	if (pvi) {
		if (mV > MID_VOLTAGE) {
			mult = VID_DIVIDOR1;
		} else {
			maxVid = MID_VID;
			volt = MID_VOLTAGE;
		}
	}
	min = (mV - (mult / 2));
	max = (mV + (mult / 2));
	for (i = 1; i <= maxVid; i++) {
		tmpv = volt - i * mult;
		if (tmpv >= min && tmpv <= max) {
			if (debug && !quiet) { printf("Found vid %d for voltage %.2f\n", i, tmpv); }
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
		error("Could not open /proc/bus/pci/00/18.3 ; Unsupported CPU? ; Do you have the required permissions to read this file?");
	}

	if (read(fh, &buff, 256) != 256) {
		close(fh);
		error("Could not read data from /proc/bus/pci/00/18.3 ; Unsupported CPU? ; Do you have the required permissions to read this file?");
	}
	close(fh);

	if (buff[3] != 0x12 || buff[2] != 0x3 || buff[1] != 0x10 || buff[0] != 0x22) {
		error("Could not find voltage encodings from /proc/bus/pci/00/18.3 ; Unsupported CPU?");
	}
	pvi = ((buff[0xa1] & 1) == 1);
}


void error(const char *message) {
	if (!quiet) {
		fprintf(stderr, "ERROR: %s\n", message);
	}
	exit(EXIT_FAILURE);
}
