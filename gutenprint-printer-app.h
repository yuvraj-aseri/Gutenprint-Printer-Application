
#ifndef GUTENPRINT_H
#  define GUTENPRINT_H

//
// Include necessary headers...
//

#  include <pappl/pappl.h>




//
// Constants...
//

#  define GUTENPRINT_TESTPAGE_MIMETYPE	"application/vnd.cups-paged-gutenprint"


//
// Functions...
//

extern bool	brf_gen(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *data, ipp_t **attrs, void *cbdata);


#endif // !GUTENPRINT_H
