#include "jobstore.h"
#include "config.h"
#include "sdcard.h"
#include <LittleFS.h>

static bool s_flashOk = false;

static void ensure_dir(fs::FS &fs)
{
    if (!fs.exists(JOBS_DIR)) fs.mkdir(JOBS_DIR);
}

// Built-in 5 min XYZ oscillation. No comments: nested parentheses in a
// previous upload leaked "first. Finger on e-stop." to GRBL (error:2).
static void seed_oscillate_job()
{
    File f = LittleFS.open(JOBS_DIR "/xyz-oscillate.nc", "w");
    if (!f) return;
    f.println("G21");
    f.println("G91");
    f.println("G94");
    f.println("F600");
    for (int i = 0; i < 125; i++) {
        f.println("G1 X8 Y8 Z4");
        f.println("G1 X-8 Y-8 Z-4");
    }
    f.println("G90");
    f.println("M30");
    f.close();
}

void store_init()
{
    s_flashOk = LittleFS.begin(true);   // format on first boot if needed
    if (s_flashOk) {
        ensure_dir(LittleFS);
        seed_oscillate_job();           // overwrite any broken copy on flash
    }
    if (store_sd_ready()) ensure_dir(sd_fs());
}

bool store_sd_ready() { return sd_available(); }

bool store_is_job_file(const String &name)
{
    int dot = name.lastIndexOf('.');
    if (dot < 0) return false;
    String ext = name.substring(dot);
    ext.toLowerCase();
    String all = JOB_EXTENSIONS;                 // ".nc,.gcode,..."
    return (String(",") + all + ",").indexOf(String(",") + ext + ",") >= 0;
}

static void list_fs(fs::FS &fs, bool onSd, const char *prefix, std::vector<JobEntry> &out)
{
    File dir = fs.open(JOBS_DIR);
    if (!dir || !dir.isDirectory()) return;
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        if (f.isDirectory()) continue;
        String nm = String(f.name());
        int slash = nm.lastIndexOf('/');
        if (slash >= 0) nm = nm.substring(slash + 1);
        if (!store_is_job_file(nm)) continue;
        JobEntry e;
        e.name  = nm;
        e.size  = f.size();
        e.onSd  = onSd;
        e.vpath = String(prefix) + "/" + nm;
        out.push_back(e);
    }
}

void store_list(std::vector<JobEntry> &out)
{
    out.clear();
    if (store_sd_ready()) list_fs(sd_fs(), true, STORE_SD_PREFIX, out);
    if (s_flashOk)        list_fs(LittleFS, false, STORE_FLASH_PREFIX, out);
}

bool store_resolve(const String &vpath, fs::FS *&fs, String &realPath)
{
    if (vpath.startsWith(STORE_SD_PREFIX "/")) {
        if (!store_sd_ready()) return false;
        fs = &sd_fs();
        realPath = String(JOBS_DIR) + "/" + vpath.substring(sizeof(STORE_SD_PREFIX));
        return true;
    }
    if (vpath.startsWith(STORE_FLASH_PREFIX "/")) {
        if (!s_flashOk) return false;
        fs = &LittleFS;
        realPath = String(JOBS_DIR) + "/" + vpath.substring(sizeof(STORE_FLASH_PREFIX));
        return true;
    }
    return false;
}

File store_open_write(const String &name, String &vpathOut)
{
    // Strip any path the browser may have included.
    String base = name;
    int slash = base.lastIndexOf('/');
    if (slash >= 0) base = base.substring(slash + 1);
    slash = base.lastIndexOf('\\');
    if (slash >= 0) base = base.substring(slash + 1);

    String real = String(JOBS_DIR) + "/" + base;
    if (store_sd_ready()) {
        vpathOut = String(STORE_SD_PREFIX) + "/" + base;
        return sd_fs().open(real, FILE_WRITE);
    }
    if (s_flashOk) {
        vpathOut = String(STORE_FLASH_PREFIX) + "/" + base;
        return LittleFS.open(real, FILE_WRITE);
    }
    return File();
}

bool store_delete(const String &vpath)
{
    fs::FS *fs = nullptr;
    String real;
    if (!store_resolve(vpath, fs, real)) return false;
    return fs->remove(real);
}
