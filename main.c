#include <stdio.h>
#include <stdbool.h>
#include <string.h>

// ── Constants ──────────────────────────────────────────────────────────────────
#define MAX_BINS 100
#define MAX_ZONE 20
#define MAX_VEHICLES 20
#define MAX_DRIVERS 20
#define ADMIN_PASS "admin123"
#define BINS_CSV "bins.csv"

// ── Waste Types ────────────────────────────────────────────────────────────────
#define DRY       1
#define WET       2
#define MIXED     3
#define HAZARDOUS 4

// ── Priority Levels ────────────────────────────────────────────────────────────
#define PRI_HIGH 3
#define PRI_MED  2
#define PRI_LOW  1

// ── WPI / Fill Thresholds ──────────────────────────────────────────────────────
#define FILL_THRESH_HIGH 70.0f   // fill% → HIGH priority
#define FILL_THRESH_MED  40.0f   // fill% → MED  priority
#define WPI_THRESH_HIGH  60.0f   // WPI   → HIGH priority
#define WPI_THRESH_MED   35.0f   // WPI   → MED  priority

// ── Bin Structure ──────────────────────────────────────────────────────────────
typedef struct {
    long long int bin_id;
    int   zone;
    int   waste_type;
    float fill_level;
    float capacity;
    float wpi;
    int   priority;
    int   x, y;
    char  last_collection[11]; // DD-MM-YYYY + '\0'
    bool  collected_today;
} Bin;

Bin bins[MAX_BINS];

// ══════════════════════════════════════════════════════════════════════════════
//  HELPERS
// ══════════════════════════════════════════════════════════════════════════════

const char* getWasteTypeString(int type) {
    switch (type) {
        case DRY:       return "DRY";
        case WET:       return "WET";
        case MIXED:     return "MIXED";
        case HAZARDOUS: return "HAZARDOUS";
        default:        return "UNKNOWN";
    }
}

const char* getPriorityString(int priority) {
    switch (priority) {
        case PRI_HIGH: return "HIGH";
        case PRI_MED:  return "MEDIUM";
        case PRI_LOW:  return "LOW";
        default:       return "UNKNOWN";
    }
}

// Maps waste-type string (from CSV) back to int constant
int parseWasteType(const char* str) {
    if (strcmp(str, "DRY")       == 0) return DRY;
    if (strcmp(str, "WET")       == 0) return WET;
    if (strcmp(str, "MIXED")     == 0) return MIXED;
    if (strcmp(str, "HAZARDOUS") == 0) return HAZARDOUS;
    return DRY; // safe default
}

float computeWPI(float fill, int wasteType) {
    int typefactor;
    switch (wasteType) {
        case DRY:       typefactor = 20; break;
        case WET:       typefactor = 40; break;
        case MIXED:     typefactor = 50; break;
        case HAZARDOUS: typefactor = 80; break;
        default:        typefactor = 20;
    }
    return (0.4f * typefactor) + (0.6f * fill);
}

// ── Check if CSV file already has a header ─────────────────────────────────────
int csvHasHeader() {
    FILE *fp = fopen(BINS_CSV, "r");
    if (!fp) return 0;
    char line[256];
    fgets(line, sizeof(line), fp);
    fclose(fp);
    // If first line starts with 'b' it's the header row
    return (line[0] == 'b');
}

// ── Generate unique Bin ID: [waste_type][zone_padded][serial] ─────────────────
long long int generateBinID(int waste_type, int zone) {
    FILE *fp = fopen(BINS_CSV, "r");

    long long int id;
    int z, collected, x, y;
    char wasteTypeStr[20];
    float fill, cap, wpi;
    int maxSerial = 0;

    if (fp == NULL) {
        return (long long)waste_type * 100000 + zone * 1000 + 1;
    }

    char line[256];
    fgets(line, sizeof(line), fp); // skip header

    while (fscanf(fp, "%lld,%d,%[^,],%f,%f,%d,%d,%f,%d",
                  &id, &z, wasteTypeStr,
                  &cap, &fill,
                  &x, &y,
                  &wpi, &collected) == 9) {
        int currentWasteType = (int)(id / 100000);
        int currentZone      = (int)((id / 1000) % 100);
        if (currentZone == zone && currentWasteType == waste_type) {
            int serial = (int)(id % 1000);
            if (serial > maxSerial) maxSerial = serial;
        }
    }
    fclose(fp);

    int newSerial = maxSerial + 1;
    if (newSerial > 999) {
        printf("Max bins reached for this zone!\n");
        return -1;
    }
    return (long long)waste_type * 100000 + (long long)zone * 1000 + newSerial;
}

// ══════════════════════════════════════════════════════════════════════════════
//  MODULE 1 : createBin()
// ══════════════════════════════════════════════════════════════════════════════
void createBin() {
    int n;
    printf("\nEnter the Number of Bins to be Created: ");
    scanf("%d", &n);

    // ── Open file; write header only if file is new/empty ─────────────────────
    FILE *fp = fopen(BINS_CSV, "a");
    if (!fp) {
        printf("Error Loading Database!\n");
        return;
    }
    if (!csvHasHeader()) {
        fprintf(fp, "bin_id,zone,waste_type,capacity,fill_level,x,y,wpi,collected_today\n");
    }

    for (int i = 0; i < n; i++) {
        printf("\n--- Enter Details for Bin %d ---", i + 1);

        // Zone
        printf("\nEnter the Bin Zone (1-%d): ", MAX_ZONE);
        scanf("%d", &bins[i].zone);
        if (bins[i].zone < 1 || bins[i].zone > MAX_ZONE) {
            printf("Invalid Bin Zone!\n");
            i--; continue;
        }

        // Waste Type
        printf("\nSelect Waste Type:\n");
        printf("  1. DRY\n  2. WET\n  3. MIXED\n  4. HAZARDOUS\n");
        printf("Option: ");
        scanf("%d", &bins[i].waste_type);
        if (bins[i].waste_type < 1 || bins[i].waste_type > 4) {
            printf("Invalid Waste Type!\n");
            i--; continue;
        }

        // Capacity
        printf("Enter Capacity (kg): ");
        scanf("%f", &bins[i].capacity);
        if (bins[i].capacity <= 0) {
            printf("Capacity must be > 0!\n");
            i--; continue;
        }

        // Fill Level
        printf("Enter Fill Level (0-100%%): ");
        scanf("%f", &bins[i].fill_level);
        if (bins[i].fill_level < 0 || bins[i].fill_level > 100) {
            printf("Invalid fill level!\n");
            i--; continue;
        }

        // Coordinates
        printf("Enter Location (x y): ");
        scanf("%d %d", &bins[i].x, &bins[i].y);

        // Generate ID
        long long int id = generateBinID(bins[i].waste_type, bins[i].zone);
        if (id < 0) { i--; continue; }

        bins[i].bin_id          = id;
        bins[i].collected_today = false;
        bins[i].wpi             = computeWPI(bins[i].fill_level, bins[i].waste_type);
        strcpy(bins[i].last_collection, "N/A");

        // ── FIX: waste_type written as string; bin_id uses %lld ───────────────
        fprintf(fp, "%lld,%d,%s,%.2f,%.2f,%d,%d,%.2f,%d\n",
                bins[i].bin_id,
                bins[i].zone,
                getWasteTypeString(bins[i].waste_type),
                bins[i].capacity,
                bins[i].fill_level,
                bins[i].x,
                bins[i].y,
                bins[i].wpi,
                (int)bins[i].collected_today);

        printf("Bin %lld created successfully!\n", bins[i].bin_id);
    }

    fclose(fp);
}

// ══════════════════════════════════════════════════════════════════════════════
//  MODULE 2 : loadBinsFromCSV()
//  Reads bins.csv → populates global bins[] → returns count
// ══════════════════════════════════════════════════════════════════════════════
int loadBinsFromCSV() {
    FILE *fp = fopen(BINS_CSV, "r");
    if (!fp) {
        printf("No bin database found. Please create bins first.\n");
        return 0;
    }

    char line[256];
    fgets(line, sizeof(line), fp); // skip header

    int count = 0;
    char wasteTypeStr[20];

    while (count < MAX_BINS &&
           fscanf(fp, "%lld,%d,%[^,],%f,%f,%d,%d,%f,%d\n",
                  &bins[count].bin_id,
                  &bins[count].zone,
                  wasteTypeStr,
                  &bins[count].capacity,
                  &bins[count].fill_level,
                  &bins[count].x,
                  &bins[count].y,
                  &bins[count].wpi,
                  (int *)&bins[count].collected_today) == 9) {

        bins[count].waste_type = parseWasteType(wasteTypeStr);
        // Recompute WPI fresh on every load for consistency
        bins[count].wpi = computeWPI(bins[count].fill_level, bins[count].waste_type);
        count++;
    }

    fclose(fp);
    return count;
}

// ── Sort bins[] by priority descending (HIGH first) ───────────────────────────
void sortBinsByPriority(int totalBins) {
    for (int i = 0; i < totalBins - 1; i++) {
        for (int j = 0; j < totalBins - i - 1; j++) {
            if (bins[j].priority < bins[j + 1].priority) {
                Bin tmp      = bins[j];
                bins[j]      = bins[j + 1];
                bins[j + 1]  = tmp;
            }
        }
    }
}

// ══════════════════════════════════════════════════════════════════════════════
//  MODULE 3 : identifyCriticalBins()
//  Loads bins, assigns priority via fill% + WPI, prints report,
//  returns count of HIGH priority bins.
// ══════════════════════════════════════════════════════════════════════════════
int identifyCriticalBins() {
    printf("\n========================================");
    printf("\n    IDENTIFY CRITICAL BINS MODULE       ");
    printf("\n========================================\n");

    // Step 1 – load from CSV
    int totalBins = loadBinsFromCSV();
    if (totalBins == 0) {
        printf("No bins available.\n");
        return 0;
    }

    int criticalCount = 0;
    int medCount      = 0;
    int lowCount      = 0;

    // Step 2 – compute WPI + assign priority (follows your flowchart exactly)
    for (int i = 0; i < totalBins; i++) {

        // Already collected today → LOW, skip further checks
        if (bins[i].collected_today) {
            bins[i].priority = PRI_LOW;
            lowCount++;
            continue;
        }

        bins[i].wpi = computeWPI(bins[i].fill_level, bins[i].waste_type);

        bool fillHigh = (bins[i].fill_level >= FILL_THRESH_HIGH);
        bool wpiHigh  = (bins[i].wpi        >= WPI_THRESH_HIGH);
        bool fillMed  = (bins[i].fill_level >= FILL_THRESH_MED);
        bool wpiMed   = (bins[i].wpi        >= WPI_THRESH_MED);

        if (fillHigh || wpiHigh) {
            bins[i].priority = PRI_HIGH;
            criticalCount++;
        } else if (fillMed || wpiMed) {
            bins[i].priority = PRI_MED;
            medCount++;
        } else {
            bins[i].priority = PRI_LOW;
            lowCount++;
        }
    }

    // Step 3 – sort HIGH → MED → LOW
    sortBinsByPriority(totalBins);

    // Step 4 – display report table
    printf("\n%-14s %-6s %-10s %-10s %-7s %-8s  %s\n",
           "Bin ID", "Zone", "WasteType", "Fill(%)", "WPI", "Priority", "Status");
    printf("------------------------------------------------------------------------\n");

    for (int i = 0; i < totalBins; i++) {
        printf("%-14lld %-6d %-10s %-10.1f %-7.2f %-8s  ",
               bins[i].bin_id,
               bins[i].zone,
               getWasteTypeString(bins[i].waste_type),
               bins[i].fill_level,
               bins[i].wpi,
               getPriorityString(bins[i].priority));

        if (bins[i].collected_today)
            printf("[Already Collected]");
        else if (bins[i].priority == PRI_HIGH)
            printf("*** NEEDS COLLECTION ***");
        else if (bins[i].priority == PRI_MED)
            printf("Monitor");
        else
            printf("OK");

        printf("\n");
    }

    printf("------------------------------------------------------------------------\n");
    printf("Total Bins     : %d\n", totalBins);
    printf("HIGH  (Critical): %d  [Fill >= %.0f%% OR WPI >= %.0f]\n",
           criticalCount, FILL_THRESH_HIGH, WPI_THRESH_HIGH);
    printf("MEDIUM          : %d  [Fill >= %.0f%% OR WPI >= %.0f]\n",
           medCount, FILL_THRESH_MED, WPI_THRESH_MED);
    printf("LOW   (OK)      : %d\n", lowCount);
    printf("========================================\n");

    return criticalCount;
}

// ══════════════════════════════════════════════════════════════════════════════
//  MAIN
// ══════════════════════════════════════════════════════════════════════════════
int main() {
    int choice;

    while (1) {
        printf("\n========== SentraBin OS ==========");
        printf("\n1. Create Bins");
        printf("\n2. Identify Critical Bins");
        printf("\n0. Exit");
        printf("\nEnter Choice: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1: createBin();           break;
            case 2: identifyCriticalBins(); break;
            case 0:
                printf("Exiting SentraBin OS. Goodbye!\n");
                return 0;
            default:
                printf("Invalid choice. Try again.\n");
        }
    }
    return 0;
}