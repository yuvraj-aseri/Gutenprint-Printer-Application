// Include necessary headers...
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <cups/cups.h>
#include <pappl/pappl.h>
#include <gutenprint/gutenprint.h> // Step 1: Include Gutenprint header


# define gutenprint_TESTPAGE_HEADER	"T*E*S*T*P*A*G*E*"
#  define gutenprint_TESTPAGE_MIMETYPE	"application/vnd.cups-paged-gutenprint"

typedef struct
{
    const char *name;
    const char *description;
    const char *device_id;
    const char *device_uri;
} pappl_pr_driver_t;

// Local functions...
static pappl_pr_driver_t *gutenprint_drivers;

// Function prototypes
static void populateGutenprintDrivers();
static const char *autoadd_cb(const char *device_info, const char *device_uri, const char *device_id, void *cbdata);
static bool driver_cb(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *data, ipp_t **attrs, void *cbdata);
static int match_id(int num_did, cups_option_t *did, const char *match_id);
static const char *mime_cb(const unsigned char *header, size_t headersize, void *data);
static bool printer_cb(const char *device_info, const char *device_uri, const char *device_id, pappl_system_t *system);
static pappl_system_t *system_cb(int num_options, cups_option_t *options, void *data);
// Local globals...


static char			gutenprint_statefile[1024];
					// State file

// 'main()' - Main entry for gutenprint.

int main(int argc, char *argv[])
{
    populateGutenprintDrivers();

    return (papplMainloop(argc, argv,
                         VERSION,
                         NULL,
                         (int)(sizeof(gutenprint_drivers) / sizeof(gutenprint_drivers[0])),
                         gutenprint_drivers, autoadd_cb, driver_cb,
                         /*subcmd_name*/NULL, /*subcmd_cb*/NULL,
                         system_cb,
                         /*usage_cb*/NULL,
                         /*data*/NULL));
}


void populateGutenprintDrivers()
{
    gutenprint_driver_t *gutenprint_driver;
    int num_drivers = gutenprint_drivers_num(NULL);
    gutenprint_drivers = (pappl_pr_driver_t *)calloc(num_drivers, sizeof(pappl_pr_driver_t));

    for (int i = 0; i < num_drivers; i++)
    {
        gutenprint_driver = gutenprint_drivers_get(NULL, i);
        if (gutenprint_driver)
        {
            pappl_pr_driver_t *driver = &gutenprint_drivers[i];
            driver->name = strdup(gutenprint_driver_name(gutenprint_driver));
            driver->description = strdup(gutenprint_driver_description(gutenprint_driver));
            driver->device_id = NULL;
            driver->device_uri = NULL;
        }
    }
}

static bool // O - `true` on success, `false` on error
driver_cb(pappl_system_t *system, // I - System
          const char *driver_name, // I - Driver name
          const char *device_uri, // I - Device URI
          const char *device_id, // I - 1284 device ID
          pappl_pr_driver_data_t *data, // I - Pointer to driver data
          ipp_t **attrs, // O - Pointer to driver attributes
          void *cbdata) // I - Callback data (not used)
{
    int i; // Looping var

    // Copy make/model info...
    for (i = 0; i < (int)(sizeof(gutenprint_drivers) / sizeof(gutenprint_drivers[0])); i++)
    {
        if (!strcmp(driver_name, gutenprint_drivers[i].name))
        {
            papplCopyString(data->make_and_model, gutenprint_drivers[i].description, sizeof(data->make_and_model));
            break;
        }
    }

    // Pages per minute
    data->ppm = 60;

    // Color values...
    data->color_supported = PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME;
    data->color_default = PAPPL_COLOR_MODE_MONOCHROME;

    // Gutenprint-specific options (vendor options)
    gutenprint_driver_t *gutenprint_driver = NULL;
    for (i = 0; i < gutenprint_drivers_num(NULL); i++)
    {
        gutenprint_driver = gutenprint_drivers_get(NULL, i);
        if (gutenprint_driver && !strcmp(gutenprint_drivers[i].name, driver_name))
            break;
    }

    if (gutenprint_driver)
    {
        // Register available paper sizes
        ipp_t *media_col_supported = NULL;
        ippAddString(system->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-col-supported", NULL, "media-col-supported", media_col_supported);

        int num_media_sizes = gutenprint_driver_media_size_count(gutenprint_driver);
        for (i = 0; i < num_media_sizes; i++)
        {
            const char *size_name = gutenprint_driver_media_size_name(gutenprint_driver, i);
            ippAddString(system->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-col-supported", NULL, size_name, media_col_supported);
        }

        // Register available resolutions
        ipp_t *print_quality_supported = NULL;
        ippAddString(system->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_RESOLUTION), "print-quality-supported", NULL, "print-quality-supported", print_quality_supported);

        int num_resolutions = gutenprint_driver_resolution_count(gutenprint_driver);
        for (i = 0; i < num_resolutions; i++)
        {
            int xres = 0, yres = 0;
            gutenprint_driver_resolution(gutenprint_driver, i, &xres, &yres);
            char res_value[32];
            snprintf(res_value, sizeof(res_value), "%dx%ddpi", xres, yres);
            ippAddResolution(system->attrs, IPP_TAG_PRINTER, "print-quality-supported", IPP_RES_PER_INCH, xres, yres);
        }

        // Register available color spaces
        ipp_t *print_color_mode_supported = NULL;
        ippAddString(system->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-color-mode-supported", NULL, "print-color-mode-supported", print_color_mode_supported);
        ippAddString(system->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-color-mode-supported", NULL, "auto", print_color_mode_supported);
        ippAddString(system->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-color-mode-supported", NULL, "color", print_color_mode_supported);
        ippAddString(system->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-color-mode-supported", NULL, "monochrome", print_color_mode_supported);

        // Add other Gutenprint vendor options as needed...

        // Call the corresponding Gutenprint driver callback to set things up
        if (!strncmp(driver_name, "gen_", 4))
            return (gutenprint_gen(system, driver_name, device_uri, device_id, data, attrs, cbdata));
        else
            return (false);
    }

    return (false);
}

// 'autoadd_cb()' - Determine the proper driver for a given printer.

static const char *autoadd_cb(const char *device_info, const char *device_uri, const char *device_id, void *cbdata)
{
    (void)device_info;
    (void)device_uri;
    (void)cbdata;

    int i, score, best_score = 0, num_did;
    cups_option_t *did;
    const char *make, *best_name = NULL;

    // First parse the device ID and get any potential driver name to match...
    num_did = papplDeviceParseID(device_id, &did);

    if ((make = cupsGetOption("MANUFACTURER", num_did, did)) == NULL)
        if ((make = cupsGetOption("MANU", num_did, did)) == NULL)
            make = cupsGetOption("MFG", num_did, did);

    // Loop through the Gutenprint drivers list to find the best match...
    for (i = 0; i < (int)(sizeof(gutenprint_drivers) / sizeof(gutenprint_drivers[0])); i++)
    {
        if (gutenprint_drivers[i].device_id)
        {
            // See if we have a matching device ID...
            score = match_id(num_did, did, gutenprint_drivers[i].device_id);
            if (score > best_score)
            {
                best_score = score;
                best_name = gutenprint_drivers[i].name;
            }
        }
    }

    // Clean up and return...
    cupsFreeOptions(num_did, did);
    return best_name;
}

static int match_id(int num_did, cups_option_t *did, const char *match_id)
{
    int i, score = 0, num_mid;
    cups_option_t *mid, *current;
    const char *value, *valptr;

    // Parse the matching device ID into key/value pairs...
    if ((num_mid = papplDeviceParseID(match_id, &mid)) == 0)
        return 0;

    // Loop through the match pairs to find matches (or not)
    for (i = num_mid, current = mid; i > 0; i--, current++)
    {
        if ((value = cupsGetOption(current->name, num_did, did)) == NULL)
        {
            // No match
            score = 0;
            break;
        }

        if (!strcasecmp(current->value, value))
        {
            // Full match!
            score += 2;
        }
        else if ((valptr = strstr(value, current->value)) != NULL)
        {
            // Possible substring match, check
            size_t mlen = strlen(current->value); // Length of match value
            if ((valptr == value || valptr[-1] == ',') && (!valptr[mlen] || valptr[mlen] == ','))
            {
                // Partial match!
                score++;
            }
            else
            {
                // No match
                score = 0;
                break;
            }
        }
        else
        {
            // No match
            score = 0;
            break;
        }
    }

    // Add libgutenprint attributes comparison here
    if (score > 0)
    {
        gutenprint_driver_t *gutenprint_driver;
        for (int j = 0; j < (int)(sizeof(gutenprint_drivers) / sizeof(gutenprint_drivers[0])); j++)
        {
            gutenprint_driver = gutenprint_drivers_get(NULL, j);
            if (gutenprint_driver && gutenprint_drivers[j].device_id)
            {
                score += match_gutenprint_attributes(num_did, did, gutenprint_driver);
            }
        }
    }

    // Clean up and return...
    cupsFreeOptions(num_mid, mid);
    return score;
}

static int match_gutenprint_attributes(int num_did, cups_option_t *did, gutenprint_driver_t *gutenprint_driver)
{
    int score = 0;
    cups_option_t *current;

    for (current = gutenprint_driver->options; current && current->name; current++)
    {
        const char *value;
        if ((value = cupsGetOption(current->name, num_did, did)) == NULL)
        {
            // No match
            score = 0;
            break;
        }

        if (!strcasecmp(current->value, value))
        {
            // Full match!
            score += 2;
        }
        else if (strstr(value, current->value) != NULL)
        {
            // Partial match!
            score++;
        }
        else
        {
            // No match
            score = 0;
            break;
        }
    }

    return score;
}

//
// 'driver_cb()' - Main driver callback.
//

static bool driver_cb(pappl_system_t *system, const char *driver_name, const char *device_uri,
                      const char *device_id, pappl_pr_driver_data_t *data, ipp_t **attrs, void *cbdata)
{
    int i; // Looping var

    // Copy make/model info...
    for (i = 0; i < (int)(sizeof(gutenprint_drivers) / sizeof(gutenprint_drivers[0])); i++)
    {
        if (!strcmp(driver_name, gutenprint_drivers[i].name))
        {
            papplCopyString(data->make_and_model, gutenprint_drivers[i].description, sizeof(data->make_and_model));
            break;
        }
    }

    // Pages per minute
    data->ppm = 60;

    // Color values...
    data->color_supported = PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME;
    data->color_default = PAPPL_COLOR_MODE_MONOCHROME;
    data->raster_types = PAPPL_PWG_RASTER_TYPE_BLACK_8; // to be done just guess

    // "print-quality-default" value...
    data->quality_default = IPP_QUALITY_NORMAL;
    data->orient_default = IPP_ORIENT_NONE;

    // "sides" values...
    data->sides_supported = PAPPL_SIDES_ONE_SIDED;
    data->sides_default = PAPPL_SIDES_ONE_SIDED;

    // "orientation-requested-default" value...
    data->orient_default = IPP_ORIENT_NONE;
    // Use the corresponding sub-driver callback to set things up...
    if (!strncmp(driver_name, "gen_", 4))
        return (gutenprint_gen(system, driver_name, device_uri, device_id, data, attrs, cbdata));

    else
        return (false);
}

//
// 'mime_cb()' - MIME typing callback...
//

static const char *			// O - MIME media type or `NULL` if none
mime_cb(const unsigned char *header,	// I - Header data
        size_t              headersize,	// I - Size of header data
        void                *cbdata)	// I - Callback data (not used)
{
    return (gutenprint_TESTPAGE_MIMETYPE); 
}

//
// 'printer_cb()' - Try auto-adding printers.
//

sstatic bool printer_cb(const char *device_info, const char *device_uri, const char *device_id, pappl_system_t *system)
{
    // Step 1: Use libgutenprint's autoadd_cb function to determine the driver name
    const char *driver_name = autoadd_cb(device_info, device_uri, device_id, system);

    if (driver_name)
    {
        char name[128];
        papplCopyString(name, device_info, sizeof(name));

        char *nameptr = strstr(name, " (");
        if (nameptr)
            *nameptr = '\0';

        if (!papplPrinterCreate(system, 0, name, driver_name, device_id, device_uri))
        {
            // Printer already exists with this name, so try adding a number to the name...
            int i;
            char newname[128];
            char number[4];
            size_t namelen = strlen(name);
            size_t numberlen;

            for (i = 2; i < 100; i++)
            {
                // Append "XXX" to the name, truncating the existing name as needed to
                // include the number at the end...
                snprintf(number, sizeof(number), " %d", i);
                numberlen = strlen(number);

                papplCopyString(newname, name, sizeof(newname));
                if ((namelen + numberlen) < sizeof(newname))
                    memcpy(newname + namelen, number, numberlen + 1);
                else
                    memcpy(newname + sizeof(newname) - numberlen - 1, number, numberlen + 1);

                // Try creating with this name...
                if (papplPrinterCreate(system, 0, newname, driver_name, device_id, device_uri))
                    break;
            }
        }
    }

    return false;
}

// 'system_cb()' - Setup the 
//system object.

static pappl_system_t * // O - System object
system_cb(int num_options, // I - Number options
          cups_option_t *options, // I - Options
          void *data) // I - Callback data (unused)
{
    pappl_system_t *system; // System object
    const char *val, *hostname, *logfile, *system_name; // Option values
    pappl_loglevel_t loglevel; // Log level
    int port = 0; // Port number, if any
    pappl_soptions_t soptions = PAPPL_SOPTIONS_MULTI_QUEUE | PAPPL_SOPTIONS_WEB_INTERFACE | PAPPL_SOPTIONS_WEB_LOG | PAPPL_SOPTIONS_WEB_SECURITY; // System options
    static pappl_version_t versions[1] = // Software versions
    {
        {"gutenprint", "", 1.0, 1.0}
    };

    // Parse standard log and server options...
    if ((val = cupsGetOption("log-level", num_options, options)) != NULL)
    {
        if (!strcmp(val, "fatal"))
            loglevel = PAPPL_LOGLEVEL_FATAL;
        else if (!strcmp(val, "error"))
            loglevel = PAPPL_LOGLEVEL_ERROR;
        else if (!strcmp(val, "warn"))
            loglevel = PAPPL_LOGLEVEL_WARN;
        else if (!strcmp(val, "info"))
            loglevel = PAPPL_LOGLEVEL_INFO;
        else if (!strcmp(val, "debug"))
            loglevel = PAPPL_LOGLEVEL_DEBUG;
        else
        {
            fprintf(stderr, "gutenprint: Bad log-level value '%s'.\n", val);
            return NULL;
        }
    }
    else
        loglevel = PAPPL_LOGLEVEL_UNSPEC;

    logfile = cupsGetOption("log-file", num_options, options);
    hostname = cupsGetOption("server-hostname", num_options, options);
    system_name = cupsGetOption("system-name", num_options, options);

    if ((val = cupsGetOption("server-port", num_options, options)) != NULL)
    {
        if (!isdigit(*val & 255))
        {
            fprintf(stderr, "gutenprint: Bad server-port value '%s'.\n", val);
            return NULL;
        }
        else
            port = atoi(val);
    }

    // State file...
    if ((val = getenv("SNAP_DATA")) != NULL)
    {
        snprintf(gutenprint_statefile, sizeof(gutenprint_statefile), "%s/gutenprint.conf", val);
    }
    else if ((val = getenv("XDG_DATA_HOME")) != NULL)
    {
        snprintf(gutenprint_statefile, sizeof(gutenprint_statefile), "%s/.gutenprint.conf", val);
    }
#ifdef _WIN32
    else if ((val = getenv("USERPROFILE")) != NULL)
    {
        snprintf(gutenprint_statefile, sizeof(gutenprint_statefile), "%s/AppData/Local/gutenprint.conf", val);
    }
    else
    {
        papplCopyString(gutenprint_statefile, "/gutenprint.ini", sizeof(gutenprint_statefile));
    }
#else
    else if ((val = getenv("HOME")) != NULL)
    {
        snprintf(gutenprint_statefile, sizeof(gutenprint_statefile), "%s/.gutenprint.conf", val);
    }
    else
    {
        papplCopyString(gutenprint_statefile, "/etc/gutenprint.conf", sizeof(gutenprint_statefile));
    }
#endif

    // Create the system object...
    if ((system = papplSystemCreate(soptions, system_name ? system_name : "gutenprint", port, "_print,_universal", cupsGetOption("spool-directory", num_options, options), logfile ? logfile : "-", loglevel, cupsGetOption("auth-service", num_options, options), /* tls_only */ false)) == NULL)
        return NULL;

    papplSystemAddListeners(system, NULL);
    papplSystemSetHostName(system, hostname);

    papplSystemSetMIMECallback(system, mime_cb, NULL); // Note this...

    papplSystemSetPrintDrivers(system, (int)(sizeof(gutenprint_drivers) / sizeof(gutenprint_drivers[0])), gutenprint_drivers, autoadd_cb, /*create_cb*/ NULL, driver_cb, system);
    papplSystemSetFooterHTML(system, "Copyright &copy; 2019-2021 by Michael R Sweet. All rights reserved.");
    papplSystemSetSaveCallback(system, (pappl_save_cb_t)papplSystemSaveState, (void *)gutenprint_statefile);
    papplSystemSetVersions(system, (int)(sizeof(versions) / sizeof(versions[0])), versions);

    fprintf(stderr, "gutenprint: statefile='%s'\n", gutenprint_statefile);

    if (!papplSystemLoadState(system, gutenprint_statefile))
    {
        // No old state, use defaults and auto-add printers...
        papplSystemSetDNSSDName(system, system_name ? system_name : "gutenprint");

        papplLog(system, PAPPL_LOGLEVEL_INFO, "Auto-adding printers...");
        papplDeviceList(PAPPL_DEVTYPE_USB, (pappl_device_cb_t)printer_cb, system, papplLogDevice, system);
    }

    // Set the operation callback
    papplSystemSetOperationCallback(system, operation_cb);

    return system;
}
