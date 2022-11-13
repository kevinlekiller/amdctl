/**
 * Copyright (C) 2015-2022  kevinlekiller
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
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>

#define MSR_PSTATE_CURRENT_LIMIT 0xc0010061
#define MSR_PSTATE_STATUS        0xc0010063
#define MSR_PSTATE_BASE          0xc0010064
#define MSR_COFVID_STATUS        0xc0010071

/* BIOS and Kernel Developer’s Guide (BKDG) For AMD Family 10h Processors
 * https://web.archive.org/web/20211030021345/https://www.amd.com/system/files/TechDocs/31116.pdf
 */
#define AMD10H 0x10 // K10
/* BIOS and Kernel Developer’s Guide (BKDG) For AMD Family 11h Processors
 * https://web.archive.org/web/20220121164239/https://www.amd.com/system/files/TechDocs/41256.pdf
 */
#define AMD11H 0x11 // Turion
/* BIOS and Kernel Developer’s Guide (BKDG) For AMD Family 12h Processors
 * https://web.archive.org/web/20211028164154/https://developer.amd.com/wordpress/media/2012/10/41131.pdf
 */
#define AMD12H 0x12 // Fusion
#define AMD13H 0x13 // Unknown
/* BIOS and Kernel Developer Guide (BKDG) for AMD Family 14h Models 00h-0Fh Processors
 * https://web.archive.org/web/20210805232321/https://www.amd.com/system/files/TechDocs/43170_14h_Mod_00h-0Fh_BKDG.pdf
 */
#define AMD14H 0x14 // Bobcat
/* BIOS and Kernel Developer’s Guide (BKDG) for AMD Family 15h Models 00h-0Fh Processors
 * https://web.archive.org/web/20220209044213/https://www.amd.com/system/files/TechDocs/42301_15h_Mod_00h-0Fh_BKDG.pdf
 * BIOS and Kernel Developer’s Guide (BKDG) for AMD Family 15h Models 10h-1Fh Processors
 * https://web.archive.org/web/20220210053552/https://www.amd.com/system/files/TechDocs/42300_15h_Mod_10h-1Fh_BKDG.pdf
 * BIOS and Kernel Developer’s Guide (BKDG) for AMD Family 15h Models 30h-3Fh Processors
 * https://web.archive.org/web/20220210071117/https://www.amd.com/system/files/TechDocs/49125_15h_Models_30h-3Fh_BKDG.pdf
 * BIOS and Kernel Developer’s Guide (BKDG) for AMD Family 15h Models 60h-6Fh Processors
 * https://web.archive.org/web/20220210071119/https://www.amd.com/system/files/TechDocs/50742_15h_Models_60h-6Fh_BKDG.pdf
 * BIOS and Kernel Developer’s Guide (BKDG) for AMD Family 15h Models 70h-7Fh Processors
 * https://web.archive.org/web/20220210071122/https://www.amd.com/system/files/TechDocs/55072_AMD_Family_15h_Models_70h-7Fh_BKDG.pdf
 */
#define AMD15H 0x15 // Bulldozer
/* BIOS and Kernel Developer’s Guide (BKDG) for AMD Family 16h Models 00h-0Fh Processors
 * https://web.archive.org/web/20220210082955/https://www.amd.com/system/files/TechDocs/48751_16h_bkdg.pdf
 * BIOS and Kernel Developer’s Guide (BKDG) for AMD Family 16h Models 30h-3Fh Processors
 * https://web.archive.org/web/20220210083005/https://www.amd.com/system/files/TechDocs/52740_16h_Models_30h-3Fh_BKDG.pdf
 */
#define AMD16H 0x16 // Jaguar
/* Processor Programming Reference (PPR) for AMD Family 17h Models 00h-0Fh Processors (PUB)
 * https://web.archive.org/web/20210805232831/https://www.amd.com/system/files/TechDocs/54945_3.03_ppr_ZP_B2_pub.zip
 * Processor Programming Reference (PPR) for AMD Family 17h Model 18h, Revision B1 Processors (PUB)
 * https://web.archive.org/web/20210805232136/https://www.amd.com/system/files/TechDocs/55570-B1-PUB.zip
 * Processor Programming Reference (PPR) for AMD Family 17h Model 71h, Revision B0 Processors (PUB)
 * https://web.archive.org/web/20220108160138/https://developer.amd.com/wp-content/resources/56176_ppr_Family_17h_Model_71h_B0_pub_Rev_3.06.zip
 * Processor Programming Reference (PPR) for AMD Family 17h Models 60h, Revision A1 Processors (PUB)
 * https://web.archive.org/web/20210805232328/https://www.amd.com/system/files/TechDocs/55922-A1-PUB.zip
 * Open-Source Register Reference for AMD Family 17h Processors (PUB)
 * https://web.archive.org/web/20220108160120/https://developer.amd.com/wp-content/resources/56255_3_03.PDF
 */
#define AMD17H 0x17 // Zen Zen+ Zen2
/* Preliminary Processor Programming Reference (PPR) for AMD Family 19h Model 21h, Revision B0 Processors (PUB)
 * https://web.archive.org/web/20210604070321/https://www.amd.com/system/files/TechDocs/56214-B0-PUB.zip
 */
#define AMD19H 0x19 // Zen3

#define PSTATE_EN_BITS        "63:63"
#define PSTATE_MAX_VAL_BITS   "6:4"
#define CUR_PSTATE_LIMIT_BITS "2:0"
#define CUR_PSTATE_BITS       "2:0"

#define MAX_VOLTAGE  1550
#define MID_VOLTAGE  1162.5
#define MAX_VID      124
#define MID_VID      63
#define MIN_VID      32
#define VID_DIVIDOR1 25
#define VID_DIVIDOR2 12.5
#define VID_DIVIDOR3 6.25

static const unsigned short REFCLK = 100;

static char *NB_VID_BITS    = "31:25";
static char *CPU_DID_BITS   = "8:6";
static char *CPU_FID_BITS   = "5:0";
static char *CPU_VID_BITS   = "15:9";
static char *IDD_DIV_BITS   = "41:40";
static char *IDD_VALUE_BITS = "39:32";

// AMD14H (Bobcat) related constants and static vars
#define COFVID_MIN_VID_BITS      "48:42"
#define COFVID_MAX_VID_BITS      "41:35"
#define ADDR_CLOCK_POWER_CONTROL "18.3"
#define MAIN_PLL_OP_FREQ_ID_BITS "5:0"
static const unsigned char REG_CLOCK_POWER_CONTROL =  0xd4;
static unsigned char       COFVID_MAX_VID          =  1;
static unsigned char       COFVID_MIN_VID          =  128;
static signed char         MAIN_PLL_COFF           = -1;

static uint64_t buffer;
static unsigned char currentOnly = 0, debug = 0, DIDS = 5, quiet = 0, PSTATES = 8, pvi = 0, testMode = 0;
static signed char cpuDid = -1, togglePs = -1;
static signed short core = -1, cores = 0, cpuFamily = 0, cpuFid = -1, cpuModel = -1, cpuVid = -1, nbVid = -1, pstate = -1;

void getCpuInfo();
void checkFamily();
void parseOpts(const int, char **);
void usage();
void fieldDescriptions();
void uwmsrCheck(const unsigned char);
void wrCpuStates();
void printCpuPstate(const unsigned char);
void printNbStates();
int getDec(const char *);
void rwMsrReg(const uint32_t, const unsigned char);
void rwPciReg(const char *, const uint32_t, const unsigned char);
void updateBuffer(const char *, const int);
void getVidType();
unsigned short vidTomV(const unsigned short);
short mVToVid(const float);
float getDiv(const int);
float getCoreMultiplier(const unsigned short, const unsigned short);
float getClockSpeed(const unsigned short, const unsigned short);
void error(const char *);

int main(const int argc, char **argv) {
	getCpuInfo();
	checkFamily();
	parseOpts(argc, argv);
	if (!quiet) {
		printf("Detected CPU model %xh, from family %xh with %d CPU cores (REFCLK = %dMHz ; Voltage ID Encodings: %s).\n", cpuModel, cpuFamily, cores, REFCLK, (pvi ? "PVI (parallel)" : "SVI (serial)"));
		if (nbVid > -1 || cpuVid > -1 || cpuFid > -1 || cpuDid > -1 || togglePs > -1) {
			printf("Preview mode %s.\n", testMode ? "On": "OFF");
		}
	}
	if (core == -1) {
		core = 0;
	} else {
		cores = core + 1;
	}
	wrCpuStates();
	printNbStates();

	return EXIT_SUCCESS;
}

/**
 * Sets some variables with info from /proc/cpuinfo
 */
void getCpuInfo() {
	FILE *fp;
	char buff[128];

	fp = fopen("/proc/cpuinfo", "r");
	if (fp == NULL) {
		error("Could not open /proc/cpuinfo for reading.");
	}

	unsigned char foundVendor = 0;
	while(fgets(buff, 128, fp)) {
		if (strstr(buff, "vendor_id") != NULL && buff[0] == 'v' && strstr(buff, "AMD") != NULL) {
			foundVendor = 1;
		} else if (strstr(buff, "cpu family") != NULL && buff[0] == 'c') {
			sscanf(buff, "%*s %*s : %hd", &cpuFamily);
		} else if (strstr(buff, "model") != NULL && buff[0] == 'm' && strstr(buff, "model name") == NULL) {
			sscanf(buff, "%*s : %hd", &cpuModel);
		} else if (strstr(buff, "siblings") != NULL && buff[0] == 's') {
			sscanf(buff, "%*s : %hd", &cores);
		}
		if (foundVendor && cpuFamily && cpuModel && cores) {
			break;
		}
	}
	fclose(fp);
	if (!foundVendor) {
		error("Processor is not an AMD?");
	}
	if (cpuModel == -1 || !cpuFamily || !cores) {
		error("Could not find CPU family or model!");
	}

	// Check for dual or quad CPU motherboards.
	unsigned short testcores = (unsigned short) sysconf(_SC_NPROCESSORS_ONLN);
	if (testcores > cores) {
		if (!quiet) {
			printf("Multi-CPU motherboard detected: CPU has %d cores, but there is a total %d cores in %d CPU sockets.\n", cores, testcores, testcores / cores);
		}
		cores = testcores;
	}
}

/**
 * Sets some variables based on current CPU model / family.
 */
void checkFamily() {
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
		case AMD14H:
			DIDS = 25;
			CPU_DID_BITS   = "8:4"; // Acutally CPU_DID_MSD
			CPU_FID_BITS   = "3:0"; // Actually CPU_DID_LSD
			rwPciReg(ADDR_CLOCK_POWER_CONTROL, REG_CLOCK_POWER_CONTROL, 1);
			MAIN_PLL_COFF = 100 * (getDec(MAIN_PLL_OP_FREQ_ID_BITS) + 16);
			core = 0;
			rwMsrReg(MSR_COFVID_STATUS, 1);
			unsigned short tmpCofvid = getDec(COFVID_MIN_VID_BITS);
			COFVID_MIN_VID = tmpCofvid ? tmpCofvid : COFVID_MIN_VID;
			tmpCofvid = getDec(COFVID_MAX_VID_BITS);
			COFVID_MAX_VID = tmpCofvid ? tmpCofvid : COFVID_MAX_VID;
			core = -1;
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
			DIDS = 0x30;
			CPU_VID_BITS   = "21:14";
			CPU_DID_BITS   = "13:8";
			CPU_FID_BITS   = "7:0";
			IDD_DIV_BITS   = "31:30";
			IDD_VALUE_BITS = "29:22";
			break;
		case AMD13H:
		default:
			fprintf(stderr, "Your CPU is not supported by amdctl (Family %xh ; Model %xh).\n", cpuFamily, cpuModel);
			exit(EXIT_FAILURE);
	}
}

/**
 * Checks options passed by user.
 */
void parseOpts(const int argc, char **argv) {
	int c;
	unsigned char allowWrites = 0, opts = 0;
	unsigned short mVolt;

	while ((c = getopt(argc, argv, "eghimstxa:c:d:f:n:p:u:v:")) != -1) {
		opts++;
		switch (c) {
			case 'a': // Toggle PState status.
				togglePs = atoi(optarg);
				if (togglePs < 0 || togglePs > 1) {
					error("Option -a must be 1 or 0.");
				}
				break;
			case 'c': // CPU core to work on.
				core = atoi(optarg);
				if (core >= cores || core < 0) {
					fprintf(stderr, "ERROR: Option -c must be less than total number of CPU cores (0 to %d).\n", cores - 1);
					exit(EXIT_FAILURE);
				}
				break;
			case 'd': // CPU did to set.
				cpuDid = atoi(optarg);
				if (cpuDid > DIDS || cpuDid < 0) {
					fprintf(stderr, "ERROR: Option -d must be a number 0 to %d.\n", DIDS);
					exit(EXIT_FAILURE);
				}
				break;
			case 'f': // CPU fid to set.
				cpuFid = atoi(optarg);
				int maxFid;
				switch (cpuFamily) {
					case AMD14H:
						maxFid = 3;
						break;
					case AMD17H:
					case AMD19H:
						maxFid = 0xc0;
						break;
					default:
						maxFid = 0x2f;
						break;
				}
				if (cpuFid > maxFid || cpuFid < 0) {
					fprintf(stderr, "ERROR: Option -f must be a number 0 to %d. You supplied %d.\n", maxFid, cpuFid);
					exit(EXIT_FAILURE);
				}
				break;
			case 'm': // Kernel >= 5.9, check / set /sys/module/msr/parameters/allow_writes to on.
				allowWrites = 1;
				break;
			case 'n': // Northbridge vid to set.
				if (cpuFamily > AMD11H) {
					error("Currently amdctl can only change the NB vid on 10h and 11h CPU's.");
				}
				nbVid = atoi(optarg);
				if (nbVid < 0 || nbVid > MAX_VID) {
					fprintf(stderr, "ERROR: Option -n must be between 0 and %d.\n", MAX_VID);
					exit(EXIT_FAILURE);
				}
				break;
			case 'p': // CPU PState to work on.
				pstate = atoi(optarg);
				if (pstate > -1 && pstate >= PSTATES) {
					fprintf(stderr, "ERROR: Option -p must be less than total number of P-States (0 to %d).\n", PSTATES - 1);
					exit(EXIT_FAILURE);
				}
				break;
			case 'u': // Finds vid based on mVolt.
				mVolt = atoi(optarg);
				if (mVolt < 1 || mVolt > MAX_VOLTAGE) {
					fprintf(stderr, "ERROR: Option -n must be between 1 and %d.\n", MAX_VOLTAGE);
					exit(EXIT_FAILURE);
				}
				short foundVid = mVToVid(mVolt);
				if (foundVid == -1) {
					printf("Could not find a vid for %dmV.\n", mVolt);
				} else {
					printf("Found vid %d for for %dmV.\n", foundVid, mVolt);
				}
				exit(EXIT_SUCCESS);
			case 'v': // Sets CPU vid.
				cpuVid = atoi(optarg);
				switch(cpuFamily) {
					case AMD14H:
						if (cpuVid < COFVID_MAX_VID || cpuVid > COFVID_MIN_VID) {
							fprintf(stderr, "ERROR: Option -v must be between %d, and %d (lower value = higher voltage).\n", COFVID_MAX_VID, COFVID_MIN_VID);
							exit(EXIT_FAILURE);
						}
						break;
					default:
						if (cpuVid < 0 || cpuVid > MAX_VID) {
							fprintf(stderr, "ERROR: Option -v must be between 0 and %d.\n", MAX_VID);
							exit(EXIT_FAILURE);
						}
				}
				break;
			case 'e': // Shows only the PState currently in use.
				currentOnly = 1;
				break;
			case 'g': // Displays CPU / Northbridge info.
				break;
			case 'i': // Show debug info.
				debug = 1;
				break;
			case 's': // Hide all console output.
				quiet = 1;
				break;
			case 't': // Preview changes only.
				testMode = 1;
				break;
			case 'x': // Displays description of various values.
				fieldDescriptions();
				break;
			case '?': // Displays help.
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

	uwmsrCheck(allowWrites);
}

/**
 * Prints help to STDOUT.
 */
void usage() {
	printf("WARNING: This software can damage your hardware, use with caution.\n");
	printf("amdctl  Copyright (C) 2015-2022  kevinlekiller  GPL-3.0-or-later\n");
	printf("Tool for under/over clocking/volting AMD CPU's.\n");
	printf("Supported AMD CPU families: 10h,11h,12h,14h,15h,16h,17h,19h\n");
	printf("These AMD CPU families are unsupported: 13h, 18h, anything older than 10h or newer than 19h\n");
	printf("Usage:\n");
	printf("amdctl [options]\n");
	printf("    -g    Get CPU and north bridge (if available) information.\n");
	printf("    -c    CPU core to work on.\n");
	printf("    -p    CPU P-state to work on.\n");
	printf("    -v    Set CPU voltage id (vid).\n");
	if (cpuFamily == AMD10H || cpuFamily == AMD11H) {
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
	printf("    -a    Activate (1) or deactivate (0) P-state.\n");
	printf("    -e    Show current P-State only. (Not available on 17h / 19h)\n");
	printf("    -t    Preview changes without applying them to the CPU / north bridge.\n");
	printf("    -u    Try to find voltage id by voltage (millivolts).\n");
	printf("    -m    On Linux kernel >= 5.9, enables userspace MSR writing.\n");
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

/**
 * Prints descriptions of names.
 */
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
	printf("REFCLK:      Used for doing some calculations, this is a fixed value, not based on the value you set in the BIOS/UEFI.\n");
	exit(EXIT_SUCCESS);
}

/**
 * Checks if userspace writing to MSR is allowed.
 * @param allowWrites -> If the user allows the program to enable /sys/module/msr/parameters/allow_writes
 */
void uwmsrCheck(const unsigned char allowWrites) {
	if (geteuid() != 0) {
		error("Root access is required to read or write from MSR's.");
	}
	struct utsname buf;
	if (uname(&buf) != 0) {
		error("Could not fetch Linux kernel information.");
	}
	short major = -1, minor = -1;
	sscanf(buf.release, "%hd.%hd", &major, &minor);
	if (major == -1 || minor == -1) {
		error("Unable to find current Linux kernel version.");
	}
	if (major < 5 || (major == 5 && minor < 9)) {
		return;
	}
	FILE *fp;
	char buff[3];
	fp = fopen("/sys/module/msr/parameters/allow_writes", "r+");
	if (fp == NULL) {
		error("Could not open /sys/module/msr/parameters/allow_writes");
	}
	if (fgets(buff, 3, fp) == NULL) {
		fclose(fp);
		error("Could not read /sys/module/msr/parameters/allow_writes");
	}
	if (strstr(buff, "on")) {
		fclose(fp);
		return;
	}
	if (!allowWrites) {
		fprintf(stderr, "ERROR: You are using Linux kernel >= 5.9 (%s) and userspace MSR writes are disabled.\n", buf.release);
		fprintf(stderr, "Set the -m option for amdctl to enable MSR userspace writing.\n");
		exit(EXIT_FAILURE);
	}
	int retVal = fputs("on", fp);
	fclose(fp);
	if (retVal < 0) {
		error("Unable to enable userspace MSR writing.\n");
	}
}

/**
 * Iterate CPU cores, get/set PState values.
 */
void wrCpuStates() {
	uint32_t tmp_pstates[PSTATES];
	unsigned char pstates_count = 0;
	if (pstate == -1) {
		for (; pstates_count < PSTATES; pstates_count++) {
			tmp_pstates[pstates_count] = (MSR_PSTATE_BASE + pstates_count);
		}
	} else { // User only requested 1 PState.
		tmp_pstates[0] = MSR_PSTATE_BASE + pstate;
		pstates_count = 1;
	}

	for (; core < cores; core++) {
		rwMsrReg(MSR_PSTATE_CURRENT_LIMIT, 1);
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
		rwMsrReg(MSR_PSTATE_STATUS, 1);
		if (!quiet) {
			printf("\nCore %d | P-State Limits (non-turbo): Highest: %d ; Lowest %d | Current P-State: %d\n", core, maxPstate, minPstate, curPstate);
			printf(" Pstate Status CpuFid CpuDid CpuVid  CpuMult     CpuFreq CpuVolt IddVal IddDiv CpuCurr CpuPower");
			printf("%s\n", (cpuFamily == AMD10H || cpuFamily == AMD11H) ? " NbVid NbVolt" : "");
		}
		if (!currentOnly) {
			for (i = 0; i < pstates_count; i++) {
				if (!quiet) {
					printf("%7d", (pstate >= 0 ? pstate : i));
				}
				rwMsrReg(tmp_pstates[i], 1);
				if (nbVid > -1 || cpuVid > -1 || cpuFid > -1 || cpuDid > -1 || togglePs > -1) {
					if (togglePs > -1) {
						updateBuffer(PSTATE_EN_BITS, togglePs);
					}
					if (nbVid > -1) {
						updateBuffer(NB_VID_BITS, nbVid);
					}
					if (cpuVid > -1) {
						updateBuffer(CPU_VID_BITS, cpuVid);
					}
					if (cpuFid > -1) {
						updateBuffer(CPU_FID_BITS, cpuFid);
					}
					if (cpuDid > -1) {
						updateBuffer(CPU_DID_BITS, cpuDid);
					}
					rwMsrReg(tmp_pstates[i], 0);
				}
				printCpuPstate(1);
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
				rwMsrReg(MSR_COFVID_STATUS, 1);
				printCpuPstate(0);
				break;
		}
	}
}

/**
 * Print CPU PState data to STDOUT.
 * @param idd -> Print CPU current/power draw or not.
 */
void printCpuPstate(const unsigned char idd) {
	const unsigned char status = (idd ? getDec(PSTATE_EN_BITS) : 1);
	const unsigned short CpuVid = getDec(CPU_VID_BITS), CpuDid = getDec(CPU_DID_BITS), CpuFid = getDec(CPU_FID_BITS);
	const unsigned short CpuVolt = vidTomV(CpuVid);
	if (!quiet) {
		if ((cpuFamily == AMD17H || cpuFamily == AMD19H) && !CpuVid) {
			printf(" disabled\n");
			return;
		}
		printf(
			"%7d%7d%7d%7d%8.2fx%9.2fMHz%6dmV",
			status, CpuFid, CpuDid, CpuVid, getCoreMultiplier(CpuFid, CpuDid), getClockSpeed(CpuFid, CpuDid), CpuVolt
		);
	}
	if (idd) {
		short IddDiv = getDec(IDD_DIV_BITS), IddVal = getDec(IDD_VALUE_BITS);
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
				IddDiv = -1;
				break;
		}
		if (IddDiv != -1) {
			float cpuCurrDraw = (cpuFamily == AMD17H || cpuFamily == AMD19H) ? IddVal + IddDiv : ((float) IddVal / (float) IddDiv);
			if (!quiet) {
				printf("%7d%7d%7.2fA%8.2fW", IddVal, IddDiv, cpuCurrDraw, ((cpuCurrDraw * CpuVolt) / 1000));
			}
		}
	}
	if (!quiet) {
		if (cpuFamily == AMD10H || cpuFamily == AMD11H) {
			const int NbVid = getDec(NB_VID_BITS);
			if (!idd) {
				printf("%7s%7s%8s%9s", "", "", "", "");
			}
			printf("%6d%5dmV", NbVid, vidTomV(NbVid));
		}
		printf("\n");
	}
}

/**
 * Print North Bridge PState data to STDOUT.
 */
void printNbStates() {
	if (quiet) {
		return;
	}
	switch (cpuFamily) {
		case AMD12H:
		case AMD14H:
		case AMD15H:
		case AMD16H:
			break;
		default: // 10h and 11h NB pstates are in the CPU pstates.
			return;
	}
	unsigned short nbvid, nbfid, nbdid;
	unsigned char nbpstates;
	printf("Northbridge:\n");
	switch (cpuFamily) {
		case AMD12H:
		case AMD14H:
			//Pstate 0 = D18F3xDC
			rwPciReg("18.3", 0xdc, 1);
			nbvid = getDec("18:12");
			printf("P-State 0: %d (vid), %5dmV\n", nbvid, vidTomV(nbvid));
			//Pstate 1 = D18F6x90
			rwPciReg("18.6", 0x90, 1);
			nbvid = getDec("14:8");
			printf("P-State 1: %d (vid), %5dmV\n", nbvid, vidTomV(nbvid));
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
			unsigned short _refclk = REFCLK;
			if (cpuModel >= 0x00 && cpuModel <= 0x0f) {
				_refclk = 2 * REFCLK;
			}
			const uint32_t addresses[][4] = {{0x160, 0x164, 0x168, 0x16c}};
			for (int nbpstate = 0; nbpstate < nbpstates; nbpstate++) {
				rwPciReg("18.5", addresses[0][nbpstate], 1);
				nbvid = ((getDec("16:10") + (getDec("21:21") << 7)));
				nbfid = getDec("7:7");
				nbdid = getDec("6:1");
				printf(
					"P-State %d: %d (vid), %d (fid), %d (did), %6dmV, %dMHz (REFCLK = %dMHz)\n",
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
		default:
			break;
	}
}

/**
 * Get decimal value from specified location in buffer.
 * @param loc -> Location in buffer to get value.
 * @return int -> The decimal value.
 */
int getDec(const char *loc) {
	uint64_t temp = buffer;
	short high, low;
	int bits;

	// From msr-tools.
	sscanf(loc, "%hd:%hd", &high, &low);
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

/**
 * Read or write data (from buffer variable) to a MSR at specified register.
 * @param reg -> Register to read or write to.
 * @param read -> 1 to read data, 0 to write data.
 */
void rwMsrReg(const uint32_t reg, const unsigned char read) {
	char path[32];
	int fh;

	if (debug && !quiet) {
		printf("DEBUG: %sing data from CPU %d at register %x\n", read ? "Read" : "Writ", core, reg);
	}

	if (!read && testMode) {
		return;
	}

	sprintf(path, "/dev/cpu/%d/msr", core);
	fh = open(path, read ? O_RDONLY : O_WRONLY);
	if (fh < 0) {
		fprintf(stderr, "ERROR: Could not open %s for %sing! Is the msr kernel module loaded?\n", path, read ? "read" : "writ");
		exit(EXIT_FAILURE);
	}

	ssize_t psize = read ? pread(fh, &buffer, 8, reg) : pwrite(fh, &buffer, sizeof buffer, reg);
	close(fh);
	if (psize != sizeof buffer) {
		fprintf(stderr, "ERROR: Could not %s data to %s\n", read ? "read" : "write", path);
		exit(EXIT_FAILURE);
	}
}

/**
 * Read or write data (from buffer variable) to specified PCI location at specified register.
 * @param loc -> PCI location to read or write to.
 * @param reg -> Register to read or write to.
 * @param read -> 1 to read data, 0 to write data.
 */
void rwPciReg(const char * loc, const uint32_t reg, const unsigned char read) {
	char path[64];
	int fh;

	if (debug && !quiet) {
		printf("DEBUG: %sing data from PCI config space address %x at location %s\n", read ? "Read" : "Writ", reg, path);
	}

	if (!read && testMode) {
		return;
	}

	sprintf(path, "/proc/bus/pci/00/%s", loc);
	fh = open(path, read ? O_RDONLY : O_WRONLY);
	if (fh < 0) {
		fprintf(stderr, "ERROR: Could not open PCI config space for %sing!\n", read ? "read" : "writ");
		exit(EXIT_FAILURE);
	}

	ssize_t psize = read ? pread(fh, &buffer, 8, reg) : pwrite(fh, &buffer, sizeof buffer, reg);
	close(fh);
	if (psize != sizeof buffer) {
		fprintf(stderr, "ERROR: Could not %s data from PCI config space!\n", read ? "read" : "write");
		exit(EXIT_FAILURE);
	}
}

/**
 * Modify buffer variable with replacement data at specified location.
 * @param loc -> Location in the buffer to overwrite data.
 * @param replacement -> New data to insert.
 */
void updateBuffer(const char *loc, const int replacement) {
	short high, low;

	sscanf(loc, "%hd:%hd", &high, &low);
	if (high == low) {
		if (replacement) {
			buffer |= (1ULL << high);
		} else {
			buffer &= ~(1ULL << high);
		}
	} else if (replacement < (2 << (high - low))) {
		buffer = (buffer & ((1 << low) - (2 << high) - 1)) | (replacement << low);
	}
}

/**
 * Check if CPU uses serial or parallel voltage encodings, sets pvi variable accordingly.
 */
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

/**
 * Converts vid to millivolts.
 * @param vid -> The vid to convert.
 * @return short -> Calculated millivolts.
 */
unsigned short vidTomV(const unsigned short vid) {
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
	if ((cpuFamily == AMD15H && ((cpuModel > 0x0f && cpuModel < 0x20) || (cpuModel > 0x2f && cpuModel < 0x40))) ||
		cpuFamily == AMD17H ||
		cpuFamily == AMD19H) {
		return (MAX_VOLTAGE - (vid * VID_DIVIDOR3));
	}

	return (MAX_VOLTAGE - (vid * VID_DIVIDOR2));
}

/**
 * Converts millivolts to vid.
 * @param mV -> Millivolts to convert.
 * @return short -> The found vid or -1 on failure.
 */
short mVToVid(const float mV) {
	unsigned short tmpvid;
	for (tmpvid = 0; tmpvid <= MAX_VID; tmpvid++) {
		if (vidTomV(tmpvid) == mV) {
			break;
		}
	}
	return (vidTomV(tmpvid) == mV ? tmpvid : -1);
}

/**
 * Returns a divisor based on the provided did, used for calculating CPU multiplier.
 * @param CpuDid -> The input did.
 * @return float -> The divisor.
 */
float getDiv(const int CpuDid) {
	switch (cpuFamily) {
		case AMD11H:
			switch (CpuDid) {
				case 1:
					return 2.0;
				case 2:
					return 4.0;
				case 3:
					return 8.0;
				default:
					return 1.0;
			}
		case AMD12H:
			switch (CpuDid) {
				case 1:
					return 1.5;
				case 2:
					return 2.0;
				case 3:
					return 3.0;
				case 4:
					return 4.0;
				case 5:
					return 6.0;
				case 6:
					return 8.0;
				case 7:
					return 12.0;
				case 8:
					return 16.0;
				case 0:
				default:
					return 1.0;
			}
		default:
			switch (CpuDid) {
				case 1:
					return 2.0;
				case 2:
					return 4.0;
				case 3:
					return 8.0;
				case 4:
					return 16.0;
				default:
					return 1.0;
			}
	}
}

/**
 * Calculates the CPU core multiplier.
 * @param CpuFid -> The core frequency id.
 * @param CpuDid -> The core divisor id.
 * @return float -> The core multiplier.
 */
float getCoreMultiplier(const unsigned short CpuFid, const unsigned short CpuDid) {
	switch (cpuFamily) {
		case AMD10H:
		case AMD15H:
		case AMD16H:
			return (float) (CpuFid + 0x10) / (float) (2 << CpuDid);
		case AMD11H:
			return (float) (CpuFid + 0x08) / (float) (2 << CpuDid);
		case AMD12H:
			return (float) (CpuFid + 0x10) / getDiv(CpuDid);
		case AMD14H:
			return getClockSpeed(CpuFid, CpuDid) / (float) REFCLK;
		case AMD17H:
		case AMD19H:
			return (float) (CpuFid * VID_DIVIDOR1) / (float) (CpuDid * VID_DIVIDOR2);
		default:
			return 0;
	}
}

/**
 * Calculates the core clock speed.
 * @param CpuFid -> The core frequency id.
 * @param CpuDid -> The core divisor id.
 * @return float -> The clock speed in MHz.
 * @NOTE: 14h uses DidMsd and DidLsd for calculation, pass DidMsd to CpuDid and DidLsd to CpuFid
 */
float getClockSpeed(const unsigned short CpuFid, const unsigned short CpuDid) {
	switch (cpuFamily) {
		case AMD10H:
		case AMD15H:
		case AMD16H:
			return (float) ((REFCLK * (CpuFid + 0x10)) >> CpuDid);
		case AMD11H:
			return (float) ((REFCLK * (CpuFid + 0x08)) >> CpuDid);
		case AMD12H:
			return (float) REFCLK * getCoreMultiplier(CpuFid, CpuDid);
		case AMD14H:
			return (float) MAIN_PLL_COFF / ((float) CpuDid + (float) CpuFid * 0.25 + 1.0);
		case AMD17H:
		case AMD19H:
			return CpuFid && CpuDid ? ((float) CpuFid / (float) CpuDid) * ((float) REFCLK * 2.0) : 0.0;
		default:
			return 0.0;
	}
}

/**
 * Print message to stderr and exit.
 * @param message -> Message to print.
 */
void error(const char *message) {
	if (!quiet) {
		fprintf(stderr, "ERROR: %s\n", message);
	}
	exit(EXIT_FAILURE);
}
