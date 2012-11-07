#include "contiki.h"

#include "net/rime.h"
#include "net/rime/mesh.h"

#include <stdio.h>
#include <string.h>

#include "debug-helper.h"

static struct mesh_conn mesh;


PROCESS(baseStation, "Base Station");
AUTOSTART_PROCESSES(&baseStation);

PROCESS_THREAD(baseStation, ev, data)
{
	

}