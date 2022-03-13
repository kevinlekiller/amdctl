/**
 * Copyright (C) 2015-2021  kevinlekiller
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

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,9,0)
	#pragma GCC warning "Incompatible kernel! MSR access deprecated by your Linux kernel! To use this program, set the kernel parameter msr.allow_writes=on or set /sys/module/msr/parameters/allow_writes to on at runtime."
#endif

void printBaseFmt(const int);
int getDec(const char *);
void getReg(const uint32_t);
void getAddr(const char *, const uint32_t);
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
void northBridge(const int);

#define PSTATE_CURRENT_LIMIT 	0xc0010061
#define PSTATE_STATUS        	0xc0010063
#define PSTATE_BASE          	0xc0010064
#define COFVID_STATUS        	0xc0010071

#define AMD10H 0x10 // K10
#define AMD11H 0x11 // Turion
#define AMD12H 0x12 // Fusion
#define AMD13H 0x13 // Unknown
#define AMD14H 0x14 // Bobcat
#define AMD15H 0x15 // Bulldozer
#define AMD16H 0x16 // Jaguar
#define AMD17H 0x17 // Zen Zen+ Zen2
#define AMD19H 0x19 // Zen3

#define PSTATE_EN_BITS        		"63:63"
#define PSTATE_MAX_VAL_BITS   		"6:4"
#define CUR_PSTATE_LIMIT_BITS 		"2:0"
#define CUR_PSTATE_BITS       		"2:0"

#define MAX_VOLTAGE  1550
#define MID_VOLTAGE  1162.5
#define MAX_VID      124
#define MID_VID      63
#define MIN_VID      32
#define VID_DIVIDOR1 25
#define VID_DIVIDOR2 12.5
#define VID_DIVIDOR3 6.25

static const int REFCLK     = 100;  // this is considered a read-only invariant!
static char *NB_VID_BITS    = "31:25";
static char *CPU_DID_BITS   = "8:6";
static char *CPU_FID_BITS   = "5:0";
static char *CPU_VID_BITS   = "15:9";
static char *IDD_DIV_BITS   = "41:40";
static char *IDD_VALUE_BITS = "39:32";


static uint64_t buffer;
static int PSTATES = 8, DIDS = 5, cpuFamily = 0, cpuModel = -1, cores = 0,
		pvi = 0, debug = 0, quiet = 0, testMode = 0, core = -1, pstate = -1;

// AMD14H (Bobcat) related constants and static vars
#define REG_CLOCK_POWER_CONTROL	      0xd4
#define COFVID_MIN_VID_BITS           "48:42"
#define COFVID_MAX_VID_BITS           "41:35"
static int COFVID_MAX_VID	      =	1;
static int COFVID_MIN_VID	      = 128;
static int mainPllCof                 = -1;
static char *ADDR_CLOCK_POWER_CONTROL =	"18.3";
static char *MAIN_PLL_OP_FREQ_ID_BITS = "5:0";

int main(int argc, char **argv) {
	getCpuInfo();

	int nv = -1, cv = -1, c, opts = 0, did = -1, fid = -1, currentOnly = 0, togglePs = -1, mVolt;

	if (!quiet) {
		printf("Voltage ID encodings: %s\n", (pvi ? "PVI (parallel)" : "SVI (serial)"));
		printf("Detected CPU model %xh, from family %xh with %d CPU cores (REFCLK = %dMHz).\n", cpuModel, cpuFamily, cores, REFCLK);
	}

        checkFamily();

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
				switch(cpuFamily) {
                                	case AMD14H:
						DIDS = 25;
                                        break;
                                }

				if (did > DIDS || did < 0) {
					if (!quiet) {
						fprintf(stderr, "ERROR: Option -d must be a number 0 to %d\n", DIDS);
					}
					exit(EXIT_FAILURE);
				}
				break;
			case 'f':
				fid = atoi(optarg);
				int maxFid;
				switch (cpuFamily) {
					case AMD17H:
					case AMD19H:
						maxFid = 0xc0;
						break;
                                 	case AMD14H:
						maxFid = 3;
                                        break;
					default:
						maxFid = 0x2f;
						break;
				}
				if (fid > maxFid || fid < 0) {
					if (!quiet) {
						fprintf(stderr, "Option -f must be a number 0 to %d", maxFid);
					}
					exit(EXIT_FAILURE);
				}
				break;
			case 'n':
				if (cpuFamily > AMD11H) {
					error("Currently amdctl can only change the NB vid on 10h and 11h CPU's.");
				}
				nv = atoi(optarg);
				if (nv < 0 || nv > MAX_VID) {
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
				mVolt = atoi(optarg);
				if (mVolt < 1 || mVolt > MAX_VOLTAGE) {
					error("Option -n must be between 1 and 1550.");
				}
				if (!quiet) {
					int foundVid =  mVToVid(mVolt);
					if (foundVid == -1) {
						printf("Could not find a vid for %dmV.\n", mVolt);
						exit(EXIT_SUCCESS);
					}
					printf("Found vid %d for for %dmV.\n", foundVid, mVolt);
				}
				exit(EXIT_SUCCESS);
			case 'v':
				cv = atoi(optarg);
				switch(cpuFamily) {
					case AMD14H:
						if(!COFVID_MAX_VID) COFVID_MAX_VID = 1;
						if(!COFVID_MIN_VID) COFVID_MIN_VID = 128;

						if( (COFVID_MAX_VID && cv < COFVID_MAX_VID) || 
                                                    (COFVID_MIN_VID && cv > COFVID_MIN_VID) ) {
							char tmpbuf[1024];
							snprintf(tmpbuf,1024,"Option -v must be between %d, and %d (lower value = higher voltage)", COFVID_MAX_VID, COFVID_MIN_VID);
							error((const char*)tmpbuf);
						}
						break;
					default:
						if (cv < 0 || cv > MAX_VID) {
							error("Option -v must be between 0 and 124.");
						}
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
		int i, curPstate = getDec(CUR_PSTATE_BITS), minPstate = getDec(PSTATE_MAX_VAL_BITS), maxPstate = getDec(CUR_PSTATE_LIMIT_BITS);
		switch (cpuFamily) {
			case AMD17H:
			case AMD19H:
				break;
			default:
				curPstate++;
				minPstate++;
				maxPstate++;
				break;
		}
		getReg(PSTATE_STATUS);
		if (!quiet) {
			printf("\nCore %d | P-State Limits (non-turbo): Highest: %d ; Lowest %d | Current P-State: %d\n", core,
				   maxPstate, minPstate, curPstate);
			if (cpuFamily == AMD10H || cpuFamily == AMD11H) {
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
		switch (cpuFamily) {
			case AMD17H:
			case AMD19H:
				break;
			default:
				if (!quiet) {
					printf("%7s", "current");
				}
				getReg(COFVID_STATUS);
				printBaseFmt(0);
				break;
		}
	}

	northBridge(nv);

	return EXIT_SUCCESS;
}

void northBridge(const int nvid) {
	if (nvid > -1) {
		return;
	}
	int nbvid, nbfid, nbdid, nbpstates;
	const uint32_t addresses[][4] = {{0x160, 0x164, 0x168, 0x16c}};
	printf("Northbridge:\n");
	switch (cpuFamily) {
		case AMD10H:
		case AMD11H:
			// These are in the CPU P-states.
			break;
		case AMD12H:
			//Pstate 0 = D18F3xDC
			getAddr("18.3", 0xdc);
			nbvid = getDec("18:12");
			printf("P-State 0: %d (vid), %5.0fmV\n", nbvid, vidTomV(nbvid));
			//Pstate 1 = D18F6x90
			getAddr("18.6", 0x90);
			nbvid = getDec("14:8");
			printf("P-State 1: %d (vid), %5.0fmV\n", nbvid, vidTomV(nbvid));
			break;
		case AMD15H:
		case AMD16H:
			//2 pstates: D18F5x160 and D18F5x164
			if (cpuFamily == AMD15H && (cpuModel >= 0x00 && cpuModel <= 0x0f)) {
				nbpstates = 2;
			}
			//4 pstates: D18F5x160, D18F5x164, D18F5x168 and D18F5x16C
			else if ((cpuFamily == AMD15H && ((cpuModel >= 0x10 && cpuModel <= 0x1f)  || (cpuModel >= 0x30 && cpuModel <= 0x3f) || (cpuModel >= 0x60 && cpuModel <= 0x6f))) ||
					(cpuFamily == AMD16H && ((cpuModel >= 0x00 && cpuModel <= 0x0f) || (cpuModel >= 0x30 && cpuModel <= 0x3f)))
			) {
				nbpstates = 4;
			} else {
				return;
			}
			// We need to be able to display the REFCLK used for the calculations
			int _refclk = REFCLK;
			if (cpuModel >= 0x00 && cpuModel <= 0x0f)
			{
				_refclk = 2 * REFCLK;
			}
			for (int nbpstate = 0; nbpstate < nbpstates; nbpstate++) {
				getAddr("18.5", addresses[0][nbpstate]);
				nbvid = ((getDec("16:10") + (getDec("21:21") << 7)));
				nbfid = getDec("7:7");
				nbdid = getDec("6:1");
				printf(
						"P-State %d: %d (vid), %d (fid), %d (did), %5.0fmV, %dMHz (REFCLK = %dMHz)\n",
						nbpstate,
						nbvid,
						nbfid,
						nbdid,
						vidTomV(nbvid),
						(_refclk * (nbdid + 0x4) >> nbfid),
						_refclk
				);
			}
			break;
		case AMD17H:
		case AMD19H:
			printf("No P-States on Zen Northbridge.\n");
			break;
		default:
			break;
	}
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
	if (cpuModel == -1 || !cpuFamily || !cores) {
		error("Could not find CPU family or model!");
	}

	// dual cpu or quad cpu motherboard patch.
	int testcores=(int)sysconf(_SC_NPROCESSORS_CONF);
	if(testcores>cores){
		printf("Multi-CPU motherboard detected: CPU has %d cores, but there is a total %d cores in %d CPU sockets\n", cores, testcores, testcores/cores);
		cores=testcores;
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
		case AMD17H:
		case AMD19H:
			CPU_VID_BITS = "21:14";
			CPU_DID_BITS = "13:8";
			CPU_FID_BITS = "7:0";
			IDD_DIV_BITS = "31:30";
			IDD_VALUE_BITS = "29:22";
			break;
		case AMD14H:
                        CPU_DID_BITS   = "8:4"; // Acutally CPU_DID_MSD
                        CPU_FID_BITS   = "3:0"; // Actually CPU_DID_LSD
                   	getAddr(ADDR_CLOCK_POWER_CONTROL, REG_CLOCK_POWER_CONTROL);
                        mainPllCof = 100 * (getDec(MAIN_PLL_OP_FREQ_ID_BITS) + 16);
			core = 0;
			getReg(COFVID_STATUS);
			COFVID_MIN_VID = getDec(COFVID_MIN_VID_BITS);
			COFVID_MAX_VID = getDec(COFVID_MAX_VID_BITS);
			core = -1;
                        break;
		case AMD13H:
		default:
			error("Your CPU family is not supported by amdctl.");
	}
}

void usage() {
	printf("WARNING: This software can damage your CPU, use with caution.\n");
	printf("Tool for under/over clocking/volting AMD CPU's.\n");
	printf("Supported AMD CPU families: 10h,11h,12h,15h,16h,17h,19h\n");
	printf("These AMD CPU families are unsupported: 13h,14h, anything older than 10h or newer than 19h\n");
	printf("Usage:\n");
	printf("amdctl [options]\n");
	printf("    -g    Get P-State information.\n");
	printf("    -c    CPU core to work on.\n");
	printf("    -p    P-state to work on.\n");
	printf("    -v    Set CPU voltage id (vid).\n");
	if (cpuFamily == AMD10H) {
		printf("    -n    Set north bridge voltage id (vid).\n");
	}
	if (cpuFamily == AMD14H) {
		printf("    -d    Set the CPU divisor ID most significant digit (CpuDidMSD).\n");
	} else {
		printf("    -d    Set the CPU divisor id (did).\n");
	}
	if (cpuFamily == AMD14H) {
		printf("    -f    Set the CPU divisor ID least significant digit (CpuDidLSD).\n");
	} else {
		printf("    -f    Set the CPU frequency id (fid).\n");
	}
	printf("    -v    Set the CPU voltage id (vid).\n");
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
	printf("IddDiv       Core current (intensity) dividor.\n");
	printf("CpuCurr:     The cpu current draw, in amps.\n");
    printf("               On 10h to 16h, the current draw is calculated as : IddVal / IddDiv\n");
    printf("               On 17h, 19h (Zen) the current draw is calculated as : IddVal + IddDiv\n");
	printf("CpuPower:    The cpu power draw, in watts.\n");
    printf("               Power draw is calculated as : (CpuCurr * CpuVolt) / 1000\n");
	exit(EXIT_SUCCESS);
}

void printBaseFmt(const int idd) {
	const int status = (idd ? getDec(PSTATE_EN_BITS) : 1);
	const int CpuVid = getDec(CPU_VID_BITS), CpuDid = getDec(CPU_DID_BITS), CpuFid = getDec(CPU_FID_BITS);
	const double CpuVolt = vidTomV(CpuVid);
	if (!quiet) {
		if ((cpuFamily == AMD17H || cpuFamily == AMD19H) && !CpuVid) {
			printf("      disabled\n");
			return;
		}
		if (cpuFamily == AMD10H || cpuFamily == AMD11H) {
			const int NbVid = getDec(NB_VID_BITS);
			printf(
					"%7d%7d%7d%7d%7.2fx%5dMHz%6.0fmV%6d%5.0fmV",
					status, CpuFid, CpuDid, CpuVid, getCpuMultiplier(CpuFid, CpuDid),
					getClockSpeed(CpuFid, CpuDid), CpuVolt, NbVid, vidTomV(NbVid)
			);
		} else {
			printf(
					"%7d%7d%7d%7d%7.2fx%5dMHz%6.0fmV",
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
		float cpuCurrDraw = (cpuFamily == AMD17H || cpuFamily == AMD19H) ? IddVal + IddDiv : ((float) IddVal / (float) IddDiv);
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

void getAddr(const char * loc, const uint32_t reg) {
	char path[64];
	int fh;

	sprintf(path, "/proc/bus/pci/00/%s", loc);

	if (debug && !quiet) { printf("DEBUG: Getting data from PCI config space address %x at location %s\n", reg, path); }

	fh = open(path, O_RDONLY);
	if (fh < 0) {
		error("Could not open PCI config space for reading!");
	}

	if (pread(fh, &buffer, 8, reg) != sizeof buffer) {
		close(fh);
		error("Could not read data from PCI config space!");
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
		case AMD17H:
		case AMD19H:
			return (CpuFid * VID_DIVIDOR1) / (CpuDid * VID_DIVIDOR2);
                case AMD14H:
                        return 0;
		default:
			return 0;
	}
}

int getClockSpeed(const int CpuFid, const int CpuDid) {
        float cpuDidMsd = CpuDid, cpuDidLsd = CpuFid;
	float coreClockDiv = 1;

	switch (cpuFamily) {
		case AMD10H:
		case AMD15H:
		case AMD16H:
			return ((REFCLK * (CpuFid + 0x10)) >> CpuDid);
		case AMD11H:
			return ((REFCLK * (CpuFid + 0x08)) >> CpuDid);
		case AMD12H:
			return (int) (REFCLK * (CpuFid + 0x10) / getDiv(CpuDid));
		case AMD17H:
		case AMD19H:
			return CpuFid && CpuDid ? (int) (((float)CpuFid / (float)CpuDid) * REFCLK * 2) : 0;
                case AMD14H:
                        coreClockDiv = (cpuDidMsd + cpuDidLsd*0.25 + 1.0f);
			return mainPllCof / coreClockDiv;
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

// Ported from k10ctl & AmdMsrTweaker
double vidTomV(const int vid) {
	if (cpuFamily == AMD10H) {
		if (pvi) {
			if (vid < MIN_VID) {
				return (MAX_VOLTAGE - vid * VID_DIVIDOR1);
			}
			return (MID_VOLTAGE - (vid > MID_VID ? MID_VID : vid) * VID_DIVIDOR2);
		}
		return (MAX_VOLTAGE - (vid > MAX_VID ? MAX_VID : vid) * VID_DIVIDOR2);
	}

	// https://github.com/mpollice/AmdMsrTweaker/blob/master/Info.cpp#L47
	if (cpuFamily == AMD17H ||
            cpuFamily == AMD19H ||
            (cpuFamily == AMD15H && ((cpuModel > 0x0f && cpuModel < 0x20) || (cpuModel > 0x2f && cpuModel < 0x40))) ||
            cpuFamily == AMD14H) {
		return (MAX_VOLTAGE - (vid * VID_DIVIDOR3));
	}

	return (MAX_VOLTAGE - (vid * VID_DIVIDOR2));
}

int mVToVid(const float mV) {
	int tmpvid;
	for (tmpvid = 0; tmpvid <= MAX_VID; tmpvid++) {
		if (vidTomV(tmpvid) == mV) {
			break;
		}
	}
	return (vidTomV(tmpvid) == mV ? tmpvid : -1);
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
