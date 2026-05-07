#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ── Constants ──────────────────────────────────────────────────────────────────
#define MAX_BINS 100
#define MAX_ZONE 20
#define MAX_VEHICLES 20
#define MAX_DRIVERS 20
#define ADMIN_PASS "admin@123"
#define BINS_CSV "bins.csv"
#define VEHICLES_CSV "vehicle.csv"
#define DRIVERS_CSV "drivers.csv"
#define NETIZEN_PASS "guest@123"
#define HUB_X 0
#define HUB_Y 0

//routes
#define ROUTES_CSV "routes.csv"
#define ADDRESSED_CSV "addressed.csv"
#define COMPLAINTS_CSV "complaints.csv"
#define EMERGENCY_CSV "emergency.csv"
#define DISTANCE_CSV "distance_matrix.csv"

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

// ── REPORT & ANALYSIS CONSTANTS ─────────────────────────────
#define FUEL_PRICE_PER_LITER 102.5f
#define DRIVER_SALARY_PER_HOUR 150.0f
#define MAINTENANCE_COST_PER_VEHICLE 2500.0f
#define RECYCLING_EFFICIENCY_TARGET 75.0f

// ── Data Structure ──────────────────────────────────────────────────────────────

typedef struct {
    long long int bin_id;
    int   zone;
    int   waste_type;
    float fill_level;
    float capacity;
    float wpi;
    int   priority;
    int   x, y;
    char  last_collection[11];
    bool  collected_today;
} Bin;

typedef struct {
    int   vehicle_id;
    char  type[30];
    float max_capacity;
    float fuel_tank_capacity;
    float current_fuel;
    float fuel_consumption_rate;
    int   zone;
    bool  is_available;
    bool  under_maintenance;
    int   assigned_driver_id;
    float current_load;
    char  registration[20];

    int current_x;
    int current_y;

    int route_bin_ids[20];
    int route_bin_count;

} Vehicle;

typedef struct{
    int   driver_id; // [Hazard Flag][Serial] 1___ - normal driver, 8___ - hazardous Certified Driver
    char  name[50];
    char  phone[11];
    int   assigned_vehicle_id; // 0 if none
    bool  is_available; // 1 if available 0 if unavailable
    bool  is_suspended; // 0 if false 1 if suspended
    bool  has_hazmat_license; // 1 if true 0 if none
    float hours_worked_today;
    float max_daily_hours;
} Driver;

// ═════════════════════════════════════════════════════════════════════
// ZONE STRUCTURE
// ═════════════════════════════════════════════════════════════════════

typedef struct {
    int zone_id;
    int total_bins;
    int critical_bins;
} Zone;

// =====================================================================
// COMPLAINT STRUCTURE
// =====================================================================

typedef struct {

    int complaint_id;

    long long int bin_id;

    int zone;

    char username[40];

    char complaint_text[150];

    int escalation_level;

    bool emergency_triggered;

    char status[20];

} Complaint;

// ═════════════════════════════════════════════════════════════════════
// EMERGENCY STRUCTURE
// ═════════════════════════════════════════════════════════════════════

typedef struct {

    int emergency_id;

    int vehicle_id;
    int driver_id;
    int zone;

    char emergency_type[50];

    char description[150];

} Emergency;

// ═════════════════════════════════════════════════════════════════════
// ROUTE STRUCTURE
// ═════════════════════════════════════════════════════════════════════

typedef struct {
    int route_id;
    int vehicle_id;
    int driver_id;
    int zone;
    float total_distance;
    float total_fuel_used;
    int bins_collected;
} Route;

// ── FUNCTION PROTOTYPES ──────────────────────────────────────────────────────────────
int loadBinsFromCSV();
void saveBinsToCSV(int total);
int loadVehiclesFromCSV();
int loadDriversFromCSV();
void saveVehiclesToCSV(int totalVehicles);
void createDriverForVehicle(int vehicleID, int zone);
void runRouteOptimization();
void viewComplaints();
void resolveComplaint();
void generateDistanceMatrix();
float calculateFuelCost(float fuelUsed);
void generateReport();
int countComplaintsForBin(long long int binID);
void emergencyHandler(long long int emergencyBinID);
void optimizeMultiBinTrip(
    int totalBins,
    int totalVehicles,
    int totalDrivers,
    FILE *fp,
    int *routeID
);

int findBackupVehicle(
    int currentZone,
    int totalVehicles
);
void optimizeVehicleRoute(int vehicleIndex, int totalBins);
int findNearestNextBin(int currentX, int currentY, int zone, int totalBins);

// ── Global Objects ──────────────────────────────────────────────────────────────
Bin bins[MAX_BINS];
Vehicle vehicles[MAX_VEHICLES];
Driver drivers[MAX_DRIVERS];

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

bool isCsvEmpty(const char* filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return true; // File doesn't exist, so it's "empty"

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fclose(fp);

    return (size == 0); // Returns true if file size is 0 bytes
}

// ═════════════════════════════════════════════════════════════════════
// MANHATTAN DISTANCE
// ═════════════════════════════════════════════════════════════════════

int calculateManhattanDistance(int x1, int y1, int x2, int y2) {

    int dx = abs(x1 - x2);
    int dy = abs(y1 - y2);

    return dx + dy;
}

// ═════════════════════════════════════════════════════════════════════
// GENERATE DISTANCE MATRIX
// ═════════════════════════════════════════════════════════════════════

void generateDistanceMatrix() {

    int total = loadBinsFromCSV();

    if (total == 0) {
        printf("No bins available.\n");
        return;
    }

    FILE *fp = fopen(DISTANCE_CSV, "w");

    if (!fp) {
        printf("Cannot create distance matrix.\n");
        return;
    }

    fprintf(fp, "FROM_BIN,TO_BIN,DISTANCE\n");

    for (int i = 0; i < total; i++) {

        for (int j = i + 1; j < total; j++) {

            int distance =
            calculateManhattanDistance(
                bins[i].x,
                bins[i].y,
                bins[j].x,
                bins[j].y
            );

            fprintf(fp,
                    "%lld,%lld,%d\n",
                    bins[i].bin_id,
                    bins[j].bin_id,
                    distance);
        }
    }

    fclose(fp);

    printf("Distance Matrix Generated Successfully.\n");
}

int distanceFromHub(int x, int y) {

    return calculateManhattanDistance(
        HUB_X,
        HUB_Y,
        x,
        y
    );
}

bool allBinsCollected(int totalBins) {

    for (int i = 0; i < totalBins; i++) {

        if (!bins[i].collected_today)
            return false;
    }

    return true;
}

int generateRouteID() {

    FILE *fp = fopen(ROUTES_CSV, "r");

    if (!fp)
        return 1;

    char line[256];
    int id;
    int maxID = 0;

    fgets(line, sizeof(line), fp);

    while (fgets(line, sizeof(line), fp)) {

        if (sscanf(line, "%d,", &id) == 1) {

            if (id > maxID)
                maxID = id;
        }
    }

    fclose(fp);

    return maxID + 1;
}

int generateComplaintID() {

    FILE *fp = fopen(COMPLAINTS_CSV, "r");

    if (!fp)
        return 1;

    char line[256];
    int id;
    int maxID = 0;

    fgets(line, sizeof(line), fp);

    while (fgets(line, sizeof(line), fp)) {

        if (sscanf(line, "%d,", &id) == 1) {

            if (id > maxID)
                maxID = id;
        }
    }

    fclose(fp);

    return maxID + 1;
}

// ═════════════════════════════════════════════════════════════════════
// SAVE DRIVERS TO CSV
// ═════════════════════════════════════════════════════════════════════

void saveDriversToCSV(int totalDrivers) {

    FILE *fp = fopen(DRIVERS_CSV, "w");

    if (!fp) {
        printf("Cannot save drivers database.\n");
        return;
    }

    fprintf(fp,
    "driver_id,name,phone,vehicle_id,available,suspended,hazmat,hours_today,max_hours\n");

    for (int i = 0; i < totalDrivers; i++) {

        fprintf(fp,
        "%d,%s,%s,%d,%d,%d,%d,%.2f,%.2f\n",

        drivers[i].driver_id,
        drivers[i].name,
        drivers[i].phone,
        drivers[i].assigned_vehicle_id,
        (int)drivers[i].is_available,
        (int)drivers[i].is_suspended,
        (int)drivers[i].has_hazmat_license,
        drivers[i].hours_worked_today,
        drivers[i].max_daily_hours);
    }

    fclose(fp);
}

// ═════════════════════════════════════════════════════════════════════
// FIND NEAREST BIN INSIDE ZONE
// ═════════════════════════════════════════════════════════════════════

int findNearestBinInZone(int zone, int currentX, int currentY, int totalBins) {

    int bestIndex = -1;

    int minDistance = 999999;

    for (int i = 0; i < totalBins; i++) {

        if (bins[i].zone != zone)
            continue;

        if (bins[i].priority != PRI_HIGH)
            continue;

        if (bins[i].collected_today)
            continue;

        int distance =
        calculateManhattanDistance(
            currentX,
            currentY,
            bins[i].x,
            bins[i].y
        );

        if (distance < minDistance) {

            minDistance = distance;

            bestIndex = i;
        }
    }

    return bestIndex;
}

int findNearestPriorityBin(
    int currentX,
    int currentY,
    int totalBins,
    int zone,
    int priority
) {

    int nearestBin = -1;

    int minimumDistance = 999999;

    for (int i = 0; i < totalBins; i++) {

        if (bins[i].collected_today)
            continue;

        if (bins[i].priority != priority)
            continue;

        if (bins[i].zone != zone)
            continue;

        int distance =
        calculateManhattanDistance(
            currentX,
            currentY,
            bins[i].x,
            bins[i].y
        );

        if (distance < minimumDistance) {

            minimumDistance = distance;

            nearestBin = i;
        }
    }

    return nearestBin;
}

// ═════════════════════════════════════════════════════════════════════
// FIND NEAREST AVAILABLE VEHICLE
// ═════════════════════════════════════════════════════════════════════

int findNearestAvailableVehicle(
    int binX,
    int binY,
    int zone,
    int totalVehicles
) {

    int bestVehicle = -1;

    int minDistance = 999999;

    // FIRST SEARCH SAME ZONE

    for (int i = 0; i < totalVehicles; i++) {

        if (vehicles[i].zone != zone)
            continue;

        if (vehicles[i].under_maintenance)
            continue;

        if (vehicles[i].assigned_driver_id == 0)
            continue;

        if (vehicles[i].current_fuel <= 0)
            continue;

        int distance =
        calculateManhattanDistance(
            HUB_X,
            HUB_Y,
            binX,
            binY
        );

        if (distance < minDistance) {

            minDistance = distance;

            bestVehicle = i;
        }
    }

    // IF NO VEHICLE FOUND INSIDE ZONE
    // SEARCH OTHER ZONES

    if (bestVehicle == -1) {

        for (int i = 0; i < totalVehicles; i++) {

            if (vehicles[i].under_maintenance)
                continue;

            if (vehicles[i].assigned_driver_id == 0)
                continue;

            if (vehicles[i].current_fuel <= 0)
                continue;

            int distance =
            calculateManhattanDistance(
                HUB_X,
                HUB_Y,
                binX,
                binY
            );

            if (distance < minDistance) {

                minDistance = distance;

                bestVehicle = i;
            }
        }
    }

    return bestVehicle;
}

int findBackupVehicle(
    int currentZone,
    int totalVehicles
) {

    int selectedVehicle = -1;

    int minimumZoneDistance = 999999;

    // FIRST SEARCH SAME ZONE

    for (int i = 0; i < totalVehicles; i++) {

        if (vehicles[i].under_maintenance)
            continue;

        if (!vehicles[i].is_available)
            continue;

        if (vehicles[i].assigned_driver_id == 0)
            continue;

        if (vehicles[i].current_fuel <= 0)
            continue;

        if (vehicles[i].zone == currentZone)
            return i;
    }

    // SEARCH OTHER ZONES

    for (int i = 0; i < totalVehicles; i++) {

        if (vehicles[i].under_maintenance)
            continue;

        if (!vehicles[i].is_available)
            continue;

        if (vehicles[i].assigned_driver_id == 0)
            continue;

        if (vehicles[i].current_fuel <= 0)
            continue;

        int zoneDistance =
        abs(vehicles[i].zone - currentZone);

        if (zoneDistance < minimumZoneDistance) {

            minimumZoneDistance = zoneDistance;

            selectedVehicle = i;
        }
    }

    return selectedVehicle;
}

void assignDriversToVehicles() {

    int totalVehicles = loadVehiclesFromCSV();
    int totalDrivers = loadDriversFromCSV();

    if (totalVehicles == 0 || totalDrivers == 0) {

        printf("Insufficient records.\n");
        return;
    }

    for (int i = 0; i < totalVehicles; i++) {

        if (vehicles[i].assigned_driver_id != 0)
            continue;

        for (int j = 0; j < totalDrivers; j++) {

            if (!drivers[j].is_available)
                continue;

            if (drivers[j].is_suspended)
                continue;

            if (drivers[j].assigned_vehicle_id != 0)
                continue;

            if (drivers[j].hours_worked_today >=
                drivers[j].max_daily_hours)
                continue;

            if (strcmp(vehicles[i].type, "Compactor") == 0 &&
                !drivers[j].has_hazmat_license)
                continue;

            vehicles[i].assigned_driver_id =
            drivers[j].driver_id;

            drivers[j].assigned_vehicle_id =
            vehicles[i].vehicle_id;

            printf(
            "Driver %d assigned to Vehicle %d\n",
            drivers[j].driver_id,
            vehicles[i].vehicle_id);

            break;
        }
    }

    saveVehiclesToCSV(totalVehicles);

    saveDriversToCSV(totalDrivers);

    printf("Driver Assignment Completed.\n");
}

// ══════════════════════════════════════════════════════════════════════════════
//  VEHICLE MANAGEMENT MODULE
// ══════════════════════════════════════════════════════════════════════════════
// ── Generate unique Vehicle ID: serial ─────────────────
int generateVehicleID() {
    FILE *fp = fopen(VEHICLES_CSV, "r");
    int maxID = 0;

    if (fp == NULL) {
        return 1001; // Start from 1001
    }

    int vehicle_id;
    char line[512];
    fgets(line, sizeof(line), fp); // skip header

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%d,", &vehicle_id) == 1) {
            if (vehicle_id > maxID) maxID = vehicle_id;
        }
    }
    fclose(fp);

    return maxID + 1;
}

void createVehicle() {

    int n;

    printf("\n========================================");
    printf("\n     CREATE VEHICLES MODULE            ");
    printf("\n========================================\n");

    printf("Enter the Number of Vehicles to Register: ");
    scanf("%d", &n);

    bool empty = isCsvEmpty(VEHICLES_CSV);

    FILE *fp = fopen(VEHICLES_CSV, "a");

    if (!fp) {
        printf("Error opening vehicle database!\n");
        return;
    }

    // CSV Header
    if (empty) {

        fprintf(fp,
        "vehicle_id,type,max_capacity,fuel_tank_capacity,current_fuel,"
        "fuel_consumption_rate,zone,is_available,"
        "under_maintenance,assigned_driver_id,current_load,"
        "registration,current_x,current_y,route_bin_count\n");
    }

    int vehicle_id = generateVehicleID();

    for (int i = 0; i < n; i++) {

        printf("\n--- Enter Details for Vehicle %d ---", i + 1);

        // ─────────────────────────────────────────────
        // Vehicle Type
        // ─────────────────────────────────────────────

        printf("\nSelect Vehicle Type:\n");
        printf("  1. Compactor   (Large capacity, high fuel consumption)\n");
        printf("  2. Tipper      (Medium capacity, moderate consumption)\n");
        printf("  3. Small       (Small capacity, low consumption)\n");
        printf("Option: ");

        int type_choice;
        scanf("%d", &type_choice);

        char type[30];

        float max_cap;
        float fuel_tank;
        float fuel_consumption;

        switch (type_choice) {

            case 1:

                strcpy(type, "Compactor");

                max_cap          = 5000.0f;
                fuel_tank        = 120.0f;
                fuel_consumption = 8.5f;

                break;

            case 2:

                strcpy(type, "Tipper");

                max_cap          = 3500.0f;
                fuel_tank        = 100.0f;
                fuel_consumption = 6.5f;

                break;

            case 3:

                strcpy(type, "Small");

                max_cap          = 1500.0f;
                fuel_tank        = 60.0f;
                fuel_consumption = 4.0f;

                break;

            default:

                printf("Invalid vehicle type!\n");

                i--;
                continue;
        }

        // ─────────────────────────────────────────────
        // Assigned Zone
        // ─────────────────────────────────────────────

        printf("\nEnter Assigned Zone (1-%d): ", MAX_ZONE);

        int zone;

        scanf("%d", &zone);

        if (zone < 1 || zone > MAX_ZONE) {

            printf("Invalid Zone!\n");

            i--;
            continue;
        }

        // ─────────────────────────────────────────────
        // Registration Number
        // ─────────────────────────────────────────────

        printf("Enter Vehicle Registration/Plate (e.g., TN-01-AB-1234): ");

        char registration[20];

        scanf("%19s", registration);

        // ─────────────────────────────────────────────
        // Default Values
        // ─────────────────────────────────────────────

        bool is_available = true;

        bool under_maintenance = false;

        int assigned_driver_id = 0;

        float current_load = 0.0f;

        float current_fuel = fuel_tank;

        int current_x = HUB_X;

        int current_y = HUB_Y;

        int route_bin_count = 0;

        // ─────────────────────────────────────────────
        // Save Vehicle to CSV
        // ─────────────────────────────────────────────

        fprintf(fp,
        "%d,%s,%.2f,%.2f,%.2f,%.2f,%d,%d,%d,%d,%.2f,%s,%d,%d,%d\n",
        vehicle_id,
        type,
        max_cap,
        fuel_tank,
        current_fuel,
        fuel_consumption,
        zone,
        (int)is_available,
        (int)under_maintenance,
        assigned_driver_id,
        current_load,
        registration,
        current_x,
        current_y,
        route_bin_count
    );

        // ─────────────────────────────────────────────
        // Success Message
        // ─────────────────────────────────────────────

        printf("\nVehicle %d (%s) registered successfully!\n",
               vehicle_id,
               type);

        printf("Registration : %s\n", registration);

        printf("Zone         : %d\n", zone);

        printf("Capacity     : %.0f kg\n", max_cap);

        printf("Fuel Tank    : %.0f liters\n", fuel_tank);

        vehicle_id++;
    }

    fclose(fp);

    printf("\n========================================\n");
}

void saveVehiclesToCSV(int totalVehicles) {

    FILE *fp = fopen(VEHICLES_CSV, "w");

    if (!fp) {

        printf("Cannot save vehicle database.\n");
        return;
    }

    fprintf(fp,
    "vehicle_id,type,max_capacity,fuel_tank_capacity,current_fuel,fuel_consumption_rate,zone,is_available,under_maintenance,assigned_driver_id,current_load,registration,current_x,current_y,route_bin_count\n");

    for (int i = 0; i < totalVehicles; i++) {

        fprintf(fp,
"%d,%s,%.2f,%.2f,%.2f,%.2f,%d,%d,%d,%d,%.2f,%s,%d,%d,%d\n",

vehicles[i].vehicle_id,
vehicles[i].type,
vehicles[i].max_capacity,
vehicles[i].fuel_tank_capacity,
vehicles[i].current_fuel,
vehicles[i].fuel_consumption_rate,
vehicles[i].zone,
(int)vehicles[i].is_available,
(int)vehicles[i].under_maintenance,
vehicles[i].assigned_driver_id,
vehicles[i].current_load,
vehicles[i].registration,
vehicles[i].current_x,
vehicles[i].current_y,
vehicles[i].route_bin_count   // ✅ NOW ACTUALLY WRITTEN
);
    }

    fclose(fp);
}

int loadVehiclesFromCSV() {

    FILE *fp = fopen(VEHICLES_CSV, "r");

    if (!fp) {
        printf("Cannot open vehicle database.\n");
        return 0;
    }

    char line[512];
    fgets(line, sizeof(line), fp); // skip header

    int count = 0;

    while (count < MAX_VEHICLES &&
           fgets(line, sizeof(line), fp) != NULL) {

        // 🔥 Remove newline + carriage return
        line[strcspn(line, "\r\n")] = 0;

        // 🔥 Skip empty lines
        if (strlen(line) == 0) continue;

        int matched = sscanf(
            line,
            "%d,%29[^,],%f,%f,%f,%f,%d,%d,%d,%d,%f,%19[^,],%d,%d,%d",

            &vehicles[count].vehicle_id,
            vehicles[count].type,
            &vehicles[count].max_capacity,
            &vehicles[count].fuel_tank_capacity,
            &vehicles[count].current_fuel,
            &vehicles[count].fuel_consumption_rate,
            &vehicles[count].zone,
            (int *)&vehicles[count].is_available,
            (int *)&vehicles[count].under_maintenance,
            &vehicles[count].assigned_driver_id,
            &vehicles[count].current_load,
            vehicles[count].registration,
            &vehicles[count].current_x,
            &vehicles[count].current_y,
            &vehicles[count].route_bin_count
        );

        if (matched == 15) {
            count++;
        } else {
            printf("❌ Skipped (parsed %d fields): %s\n", matched, line);
        }
           }

    fclose(fp);

    printf(" Loaded %d vehicles\n", count);
    return count;
}

void viewVehicles() {
    printf("\n========================================");
    printf("\n      VIEW ALL VEHICLES MODULE         ");
    printf("\n========================================\n");

    int totalVehicles = loadVehiclesFromCSV();
    if (!totalVehicles) {
        printf("No vehicles registered yet.\n");
        return;
    }

    printf("\n%-12s %-12s %-10s %-10s %-8s %-9s  %s\n",
           "Vehicle ID", "Type", "Capacity", "Fuel Tank", "Zone", "Status", "Registration");
    printf("--------------------------------------------------------------------------------------\n");

    for (int i = 0; i < totalVehicles; i++) {
        printf("%-12d %-12s %-10.0f %-10.0f %-8d %-9s %-15s\n",
               vehicles[i].vehicle_id,
               vehicles[i].type,
               vehicles[i].max_capacity,
               vehicles[i].fuel_tank_capacity,
               vehicles[i].zone,
               vehicles[i].under_maintenance ? "MAINTENANCE" :
               (vehicles[i].is_available ? "AVAILABLE" : "IN USE"),
               vehicles[i].registration);
    }

    printf("--------------------------------------------------------------------------------------\n");
    printf("Total Vehicles: %d\n", totalVehicles);

    // Count status summary
    int available_count = 0, in_use_count = 0, maintenance_count = 0;
    for (int i = 0; i < totalVehicles; i++) {
        if (vehicles[i].under_maintenance)
            maintenance_count++;
        else if (vehicles[i].is_available)
            available_count++;
        else
            in_use_count++;
    }

    printf("\nStatus Summary:\n");
    printf("  [*] Available  : %d\n", available_count);
    printf("  [=] In Use     : %d\n", in_use_count);
    printf("  [!] Maintenance: %d\n", maintenance_count);
    printf("========================================\n");
}
// ══════════════════════════════════════════════════════════════════════════════
//  BIN MANAGEMENT MODULE
// ══════════════════════════════════════════════════════════════════════════════
// ── Generate unique Bin ID: [waste_type][zone_padded][serial] ─────────────────
long long int generateBinID(int waste_type, int zone) {
    FILE *fp = fopen(BINS_CSV, "r");

    long long int id;
    int z, collected, x, y;
    char wasteTypeStr[20];
    float capacity, fill, wpi;

    int maxSerial = 0;

    if (fp == NULL) {
        return (long long)waste_type * 100000 + zone * 1000 + 1;
    }

    char line[256];
    fgets(line, sizeof(line), fp); // skip header

    char lastCollection[20];

while (fscanf(fp,
"%lld,%d,%[^,],%f,%f,%d,%d,%f,%[^,],%d\n",

&id,
&z,
wasteTypeStr,
&capacity,
&fill,
&x,
&y,
&wpi,
lastCollection,
&collected) == 10) {

        int currentWasteType = id / 100000;
        int currentZone      = (id / 1000) % 100;

        if (currentZone == zone && currentWasteType == waste_type) {
            int serial = id % 1000;
            if (serial > maxSerial) {
                maxSerial = serial;
            }
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

void createBin() {
    int n;
    printf("\nEnter the Number of Bins to be Created: ");
    scanf("%d", &n);

    // ── Open file; write header only if file is new/empty ─────────────────────
    bool empty = isCsvEmpty(BINS_CSV);
    FILE *fp = fopen(BINS_CSV, "a");
    if (empty)
    {
        fprintf(fp,
        "bin_id,zone,waste_type,fill_level,capacity,wpi,priority,x,y,last_collection,collected_today\n");
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
        bins[i].wpi        = computeWPI(bins[i].fill_level, bins[i].waste_type);
        strcpy(bins[i].last_collection, "N/A");

        // ── FIX: waste_type written as string; bin_id uses %lld ───────────────
        fprintf(fp,
"%lld,%d,%s,%.2f,%.2f,%.2f,%d,%d,%d,%s,%d\n",
bins[i].bin_id,
bins[i].zone,
getWasteTypeString(bins[i].waste_type),
bins[i].fill_level,
bins[i].capacity,
bins[i].wpi,
bins[i].priority,
bins[i].x,
bins[i].y,
bins[i].last_collection,
(int)bins[i].collected_today
);

        printf("Bin %lld created successfully!\n", bins[i].bin_id);
    }

    fclose(fp);
}

// ═════════════════════════════════════════════════════════════════════
// LOAD BINS FROM CSV
// ═════════════════════════════════════════════════════════════════════

int loadBinsFromCSV() {

    FILE *fp = fopen("bins.csv", "r");

    if (!fp) {
        printf("❌ ERROR: bins.csv not found!\n");
        return 0;
    }

    char line[256];

    // Skip header
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return 0;
    }

    int count = 0;
    char wasteTypeStr[20];
    int collected;

    while (fgets(line, sizeof(line), fp)) {

        if (count >= MAX_BINS) break;

        // 🔥 Remove newline (VERY IMPORTANT)
        line[strcspn(line, "\n")] = 0;

        int result = sscanf(line,
            "%lld,%d,%[^,],%f,%f,%f,%d,%d,%d,%[^,],%d",
            &bins[count].bin_id,
            &bins[count].zone,
            wasteTypeStr,
            &bins[count].fill_level,
            &bins[count].capacity,
            &bins[count].wpi,
            &bins[count].priority,
            &bins[count].x,
            &bins[count].y,
            bins[count].last_collection,
            &collected
        );

        if (result != 11) {
            printf("❌ Parse failed (%d fields): %s\n", result, line);
            continue;
        }

        bins[count].waste_type = parseWasteType(wasteTypeStr);
        bins[count].collected_today = (bool)collected;

        count++;
    }

    fclose(fp);

    printf(" Loaded %d bins\n", count);
    return count;
}

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
//  DRIVER MANAGEMENT MODULE
// ══════════════════════════════════════════════════════════════════════════════
int generateDriverID(bool hasHazmat) {
    FILE *fp = fopen(DRIVERS_CSV, "r");

    // Set the prefix based on certification
    // 1000 for normal, 8000 for hazardous
    int prefix = hasHazmat ? 8000 : 1000;
    int maxSerial = 0;

    // If the file doesn't exist yet, start with the first ID (e.g., 1001 or 8001)
    if (fp == NULL) {
        return prefix + 1;
    }

    char line[512];
    // Read and skip the header line
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return prefix + 1;
    }

    int existingID;
    // Scan the first column (ID) of every row
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%d,", &existingID) == 1) {

            // If we are looking for a Hazmat ID and found an 8xxx ID
            if (hasHazmat && existingID >= 8000) {
                int serial = existingID - 8000;
                if (serial > maxSerial) maxSerial = serial;
            }
            // If we are looking for a Standard ID and found a 1xxx ID
            else if (!hasHazmat && existingID >= 1000 && existingID < 8000) {
                int serial = existingID - 1000;
                if (serial > maxSerial) maxSerial = serial;
            }
        }
    }

    fclose(fp);

    // Return the prefix + the next available serial number
    return prefix + (maxSerial + 1);
}
void createDriver() {
    int n;
    printf("\n========================================");
    printf("\n     CREATE DRIVER RECORDS MODULE       ");
    printf("\n========================================\n");
    printf("\nEnter the No of Driver Records to Be Created: ");
    scanf("%d", &n);

    int totalVehicles = loadVehiclesFromCSV();

    printf("\n=========== UNASSIGNED VEHICLES ===========\n");

    for (int i = 0; i < totalVehicles; i++) {

        if (vehicles[i].assigned_driver_id == 0) {

            printf("Vehicle ID : %d | Zone : %d | Type : %s\n",

               vehicles[i].vehicle_id,
               vehicles[i].zone,
               vehicles[i].type);
            }
        }

    FILE *fp = fopen(DRIVERS_CSV, "a");
    if (!fp) {
        printf("Error: Could not open driver database.\n");
        return;
    }

    // Use our universal helper to check for header
    if (isCsvEmpty(DRIVERS_CSV)) {
        fprintf(fp, "driver_id,name,phone,vehicle_id,available,suspended,hazmat,hours_today,max_hours\n");
    }

    for (int i = 0; i < n; i++) {
        Driver d;
        int hazChoice;

        printf("\n--- Driver %d Details ---", i + 1);

        printf("\nEnter Name: ");
        scanf(" %[^\n]", d.name); // Space before % clears input buffer

        printf("Enter 10-Digit Phone Number: ");
        scanf("%10s", d.phone);

        printf("Hazardous Handling Certified? (1=Yes, 0=No): ");
        scanf("%d", &hazChoice);
        d.has_hazmat_license = (hazChoice == 1);

        // Generate ID based on the specialized sequence
        d.driver_id = generateDriverID(d.has_hazmat_license);

        printf("Enter Max Daily Working Hours: ");
        scanf("%f", &d.max_daily_hours);
        if (d.max_daily_hours > 12 || d.max_daily_hours < 1) {
            printf("\nMax Daily Working Hours Cannot be greater than 12 hours; Enter Max Daily Working Hours again: ");
            scanf("%f", &d.max_daily_hours);
        }
        // System Defaults for new drivers
        d.assigned_vehicle_id = 0;
        d.is_available = true;
        d.is_suspended = false;
        d.hours_worked_today = 0.0f;

        // Write to CSV
        fprintf(fp, "%d,%s,%s,%d,%d,%d,%d,%.2f,%.2f\n",
                d.driver_id,
                d.name,
                d.phone,
                d.assigned_vehicle_id,
                (int)d.is_available,
                (int)d.is_suspended,
                (int)d.has_hazmat_license,
                d.hours_worked_today,
                d.max_daily_hours);
        fflush(fp);
        printf("Success! Driver ID %d generated for %s.\n", d.driver_id, d.name);
    }

    fclose(fp);
}

void createDriverForVehicle(int vehicleID, int zone) {

    FILE *fp = fopen(DRIVERS_CSV, "a");

    if (!fp) {
        printf("Cannot open driver database.\n");
        return;
    }

    if (isCsvEmpty(DRIVERS_CSV)) {

        fprintf(fp,
        "driver_id,name,phone,vehicle_id,available,suspended,hazmat,hours_today,max_hours\n");
    }

    Driver d;

    int hazChoice;

    printf("\n========================================");
    printf("\n AUTO DRIVER CREATION FOR VEHICLE %d", vehicleID);
    printf("\n========================================\n");

    printf("Vehicle Assigned Zone : %d\n", zone);

    printf("Enter Driver Name: ");
    scanf(" %[^\n]", d.name);

    printf("Enter Phone Number: ");
    scanf("%s", d.phone);

    printf("Hazmat Certified? (1=YES 0=NO): ");
    scanf("%d", &hazChoice);

    d.has_hazmat_license = hazChoice;

    d.driver_id =
    generateDriverID(d.has_hazmat_license);

    d.assigned_vehicle_id = vehicleID;

    d.is_available = false;

    d.is_suspended = false;

    d.hours_worked_today = 0;

    printf("Enter Max Daily Hours: ");
    scanf("%f", &d.max_daily_hours);

    fprintf(fp,
    "%d,%s,%s,%d,%d,%d,%d,%.2f,%.2f\n",

    d.driver_id,
    d.name,
    d.phone,
    d.assigned_vehicle_id,
    (int)d.is_available,
    (int)d.is_suspended,
    (int)d.has_hazmat_license,
    d.hours_worked_today,
    d.max_daily_hours);

    fclose(fp);

    // UPDATE VEHICLE CSV ALSO

    int totalVehicles = loadVehiclesFromCSV();

    for (int i = 0; i < totalVehicles; i++) {

        if (vehicles[i].vehicle_id == vehicleID) {

            vehicles[i].assigned_driver_id =
            d.driver_id;

            vehicles[i].is_available = false;

            break;
        }
    }

    FILE *vf = fopen(VEHICLES_CSV, "w");

    fprintf(vf,
    "vehicle_id,type,max_capacity,fuel_tank_capacity,current_fuel,"
    "fuel_consumption_rate,zone,is_available,"
    "under_maintenance,assigned_driver_id,current_load,registration,current_x,current_y,route_bin_count\n");

    for (int i = 0; i < totalVehicles; i++) {

        fprintf(vf,
        "%d,%s,%.2f,%.2f,%.2f,%.2f,%d,%d,%d,%d,%.2f,%s,%d,%d,%d\n",

        vehicles[i].vehicle_id,
        vehicles[i].type,
        vehicles[i].max_capacity,
        vehicles[i].fuel_tank_capacity,
        vehicles[i].current_fuel,
        vehicles[i].fuel_consumption_rate,
        vehicles[i].zone,
        (int)vehicles[i].is_available,
        (int)vehicles[i].under_maintenance,
        vehicles[i].assigned_driver_id,
        vehicles[i].current_load,
        vehicles[i].registration,
        vehicles[i].current_x,
        vehicles[i].current_y,
        vehicles[i].route_bin_count);
    }
    fclose(vf);
    printf("\nDriver Created And Assigned Successfully!\n");
}

int loadDriversFromCSV() {
    FILE *fp = fopen(DRIVERS_CSV, "r");
    if (!fp) return 0;

    char line[512];
    if (fgets(line, sizeof(line), fp) == NULL) { // Skip header safely
        fclose(fp);
        return 0;
    }

    int count = 0;
    // Added commas after %[^,] to clear them from the buffer
    while (count < MAX_DRIVERS &&
           fscanf(fp, "%d,%[^,],%[^,],%d,%d,%d,%d,%f,%f\n",
                  &drivers[count].driver_id,
                  drivers[count].name,
                  drivers[count].phone,
                  &drivers[count].assigned_vehicle_id,
                  (int *)&drivers[count].is_available,
                  (int *)&drivers[count].is_suspended,
                  (int *)&drivers[count].has_hazmat_license,
                  &drivers[count].hours_worked_today,
                  &drivers[count].max_daily_hours) == 9) {
        count++;
                  }

    fclose(fp);
    return count;
}

void viewDrivers() {
    printf("\n========================================");
    printf("\n      VIEW ALL DRIVERS MODULE          ");
    printf("\n========================================\n");

    int total = loadDriversFromCSV();
    if (total == 0) {
        printf("No driver records found.\n");
        return;
    }

    printf("%-10s %-15s %-12s %-10s %-8s %-10s\n",
           "ID", "Name", "Phone", "Vehicle", "Hazmat", "Status");
    printf("----------------------------------------------------------------------\n");

    for (int i = 0; i < total; i++) {
        printf("%-10d %-15s %-12s %-10d %-8s %-10s\n",
               drivers[i].driver_id,
               drivers[i].name,
               drivers[i].phone,
               drivers[i].assigned_vehicle_id,
               drivers[i].has_hazmat_license ? "YES" : "NO",
               drivers[i].is_suspended ? "SUSPENDED" : (drivers[i].is_available ? "READY" : "ON-ROUTE"));
    }
    printf("----------------------------------------------------------------------\n");
}

// ══════════════════════════════════════════════════════════════════════════════
//  ROUTE OPTIMIZATION MODULE - ADMIN
// ══════════════════════════════════════════════════════════════════════════════

void optimizeMultiBinTrip(
    int totalBins,
    int totalVehicles,
    int totalDrivers,
    FILE *fp,
    int *routeID
) {

    int priorities[3] = {
        PRI_HIGH,
        PRI_MED,
        PRI_LOW
    };

    while (!allBinsCollected(totalBins)) {

        bool anyBinCollected = false;

        for (int p = 0; p < 3; p++) {

            int currentPriority = priorities[p];

            for (int v = 0; v < totalVehicles; v++) {

                if (vehicles[v].under_maintenance)
                    continue;

                if (vehicles[v].assigned_driver_id == 0)
                    continue;

                if (vehicles[v].current_fuel <= 0)
                    continue;

                vehicles[v].is_available = false;

                int currentX = HUB_X;
                int currentY = HUB_Y;

                int binsCollected = 0;

                int totalDistance = 0;

                float totalFuelUsed = 0;

                int testBin =
findNearestPriorityBin(
    currentX,
    currentY,
    totalBins,
    vehicles[v].zone,
    currentPriority
);

if (testBin == -1)
    continue;

                printf(
                "\n================================");

                printf(
                "\nVehicle %d started in Zone %d",
                vehicles[v].vehicle_id,
                vehicles[v].zone);

                while (1) {

                    int nearestBin =
                    findNearestPriorityBin(
                        currentX,
                        currentY,
                        totalBins,
                        vehicles[v].zone,
                        currentPriority
                    );

                    // SEARCH OTHER ZONES

                    if (nearestBin == -1) {

                        for (int z = 1; z <= MAX_ZONE; z++) {

                            nearestBin =
                            findNearestPriorityBin(
                                currentX,
                                currentY,
                                totalBins,
                                z,
                                currentPriority
                            );

                            if (nearestBin != -1)
                                break;
                        }
                    }

                    // NO BIN FOUND

                    if (nearestBin == -1)
                        break;

                    int distance =
                    calculateManhattanDistance(
                        currentX,
                        currentY,
                        bins[nearestBin].x,
                        bins[nearestBin].y
                    );

                    int returnDistance =
                    calculateManhattanDistance(
                        bins[nearestBin].x,
                        bins[nearestBin].y,
                        HUB_X,
                        HUB_Y
                    );

                    int futureDistance =
                    totalDistance +
                    distance +
                    returnDistance;

                    float futureFuel =
                    (futureDistance / 10.0f) *
                    vehicles[v].fuel_consumption_rate;

                    printf(
"\nDEBUG -> Vehicle:%d Fuel:%.2f Needed:%.2f Distance:%d",
vehicles[v].vehicle_id,
vehicles[v].current_fuel,
futureFuel,
futureDistance);

                    // LOW FUEL

                    if (futureFuel >
                        vehicles[v].current_fuel) {

                        printf(
                        "\nVehicle %d returned to HUB due to low fuel.",
                        vehicles[v].vehicle_id);

                        vehicles[v].current_x = HUB_X;
                        vehicles[v].current_y = HUB_Y;

                        vehicles[v].is_available = true;

                        int backupVehicle =
                        findBackupVehicle(
                            vehicles[v].zone,
                            totalVehicles
                        );

                        if (backupVehicle != -1 &&
                            backupVehicle != v) {

                            printf(
                            "\nBackup Vehicle %d dispatched.",
                            vehicles[backupVehicle].vehicle_id);
                        }

                        break;
                    }

                        // COLLECT BIN

                        bins[nearestBin].collected_today = true;

                    // EMPTY BIN AFTER COLLECTION

bins[nearestBin].fill_level = 0.0f;

bins[nearestBin].wpi = computeWPI(
    bins[nearestBin].fill_level,
    bins[nearestBin].waste_type
);

bins[nearestBin].priority = PRI_LOW;

                    printf(
                    "\nVehicle %d collected Bin %lld",
                    vehicles[v].vehicle_id,
                    bins[nearestBin].bin_id);

                    totalDistance += distance;

                    currentX = bins[nearestBin].x;
                    currentY = bins[nearestBin].y;

                    vehicles[v].current_x = currentX;
                    vehicles[v].current_y = currentY;

                    binsCollected++;

                    anyBinCollected = true;
                }

                // RETURN TO HUB

                totalDistance +=
                calculateManhattanDistance(
                    currentX,
                    currentY,
                    HUB_X,
                    HUB_Y
                );

                vehicles[v].current_x = HUB_X;
                vehicles[v].current_y = HUB_Y;

                totalFuelUsed =
                (totalDistance / 10.0f) *
                vehicles[v].fuel_consumption_rate;

                vehicles[v].current_fuel -=
                totalFuelUsed;

                if (vehicles[v].current_fuel < 0)
                    vehicles[v].current_fuel = 0;

                // UPDATE DRIVER HOURS

                for (int d = 0; d < totalDrivers; d++) {

                    if (drivers[d].driver_id ==
                        vehicles[v].assigned_driver_id) {

                        drivers[d].hours_worked_today += 2;

                        break;
                    }
                }

                vehicles[v].is_available = true;

                // SAVE ROUTE

                fprintf(fp,
                "%d,%d,%d,%d,%d,%.2f,%d\n",

                (*routeID)++,

                vehicles[v].vehicle_id,

                vehicles[v].assigned_driver_id,

                vehicles[v].zone,

                totalDistance,

                totalFuelUsed,

                binsCollected);

                printf("\nReturned to HUB.");

                printf("\nTrip Completed.");

                printf(
                "\nPriority : %s",
                getPriorityString(currentPriority));

                printf(
                "\nBins Collected : %d",
                binsCollected);

                printf(
                "\nTotal Distance : %d km",
                totalDistance);

                printf(
                "\nFuel Used : %.2f liters\n",
                totalFuelUsed);
            }
        }

        // SAFETY CHECK

        if (!anyBinCollected) {

            printf(
            "\nNo reachable bins remaining.");

            break;
        }
    }

    printf(
"\n================================");

if (allBinsCollected(totalBins)) {

    printf(
    "\nALL BINS SUCCESSFULLY COLLECTED.");
}
else {

    printf(
    "\nSOME BINS COULD NOT BE COLLECTED.");
}

printf(
"\n================================\n");
}

void runRouteOptimization() {

    int totalBins = loadBinsFromCSV();

    int totalVehicles = loadVehiclesFromCSV();

    int totalDrivers = loadDriversFromCSV();

    if (totalBins == 0 ||
        totalVehicles == 0 ||
        totalDrivers == 0) {

        printf("Insufficient data.\n");
        return;
    }
    for (int i = 0; i < totalBins; i++) {

    bins[i].collected_today = false;
}

    identifyCriticalBins();

    bool empty = isCsvEmpty(ROUTES_CSV);

    FILE *fp = fopen(ROUTES_CSV, "a");

    if (!fp) {

        printf("Cannot open routes file.\n");
        return;
    }

    if (empty) {

        fprintf(fp,
        "route_id,vehicle_id,driver_id,zone,total_distance,total_fuel,bins_collected\n");
    }

    int routeID = generateRouteID();

    optimizeMultiBinTrip(
        totalBins,
        totalVehicles,
        totalDrivers,
        fp,
        &routeID
    );

    fclose(fp);

    saveBinsToCSV(totalBins);

    saveVehiclesToCSV(totalVehicles);

    saveDriversToCSV(totalDrivers);

    printf("\nDynamic Route Optimization Completed.\n");
}

// ═════════════════════════════════════════════════════════════════════
// EMERGENCY HANDLER MODULE
// ═════════════════════════════════════════════════════════════════════

void emergencyHandler(long long int emergencyBinID)
{
    int totalBins = loadBinsFromCSV();
    int totalVehicles = loadVehiclesFromCSV();
    int totalDrivers = loadDriversFromCSV();

    if (totalBins == 0 ||
        totalVehicles == 0 ||
        totalDrivers == 0)
    {
        printf("Insufficient data for emergency handling.\n");
        return;
    }

    int emergencyBinIndex = -1;

    // FIND EMERGENCY BIN

    for (int i = 0; i < totalBins; i++)
    {
        if (bins[i].bin_id == emergencyBinID)
        {
            emergencyBinIndex = i;
            break;
        }
    }

    if (emergencyBinIndex == -1)
    {
        printf("Emergency bin not found.\n");
        return;
    }

    printf("\n========================================");
    printf("\n        EMERGENCY HANDLER ACTIVE        ");
    printf("\n========================================\n");

    printf("Emergency Bin ID : %lld\n",
           bins[emergencyBinIndex].bin_id);

    printf("Zone             : %d\n",
           bins[emergencyBinIndex].zone);

    printf("Waste Type       : %s\n",
           getWasteTypeString(
               bins[emergencyBinIndex].waste_type));

    printf("Fill Level       : %.2f%%\n",
           bins[emergencyBinIndex].fill_level);

    printf("Priority         : %s\n",
           getPriorityString(
               bins[emergencyBinIndex].priority));

    printf("WPI              : %.2f\n",
           bins[emergencyBinIndex].wpi);

    // ───────────────────────────────────────
    // FIND SUITABLE VEHICLE
    // ───────────────────────────────────────

    int selectedVehicle = -1;

    int minimumDistance = 999999;

    for (int i = 0; i < totalVehicles; i++)
    {
        // VEHICLE MUST BE AVAILABLE

        if (!vehicles[i].is_available)
            continue;

        // VEHICLE MUST NOT BE UNDER MAINTENANCE

        if (vehicles[i].under_maintenance)
            continue;

        // VEHICLE MUST HAVE DRIVER

        if (vehicles[i].assigned_driver_id == 0)
            continue;

        // VEHICLE MUST HAVE FUEL

        if (vehicles[i].current_fuel <= 0)
            continue;

        // VEHICLE MUST BELONG TO SAME ZONE

        if (vehicles[i].zone !=
            bins[emergencyBinIndex].zone)
            continue;

        // VEHICLE LOAD LIMIT

        if (vehicles[i].current_load >=
            vehicles[i].max_capacity)
            continue;

        // HAZARDOUS BIN REQUIRES COMPACTOR

        if (bins[emergencyBinIndex].waste_type ==
            HAZARDOUS)
        {
            if (strcmp(vehicles[i].type,
                       "Compactor") != 0)
                continue;
        }

        // DISTANCE BETWEEN VEHICLE & BIN

        int distance =
        calculateManhattanDistance(
            vehicles[i].current_x,
            vehicles[i].current_y,
            bins[emergencyBinIndex].x,
            bins[emergencyBinIndex].y
        );

        // FUEL REQUIRED

        float fuelNeeded =
        ((distance * 2) / 10.0f) *
        vehicles[i].fuel_consumption_rate;

        // CHECK FUEL AVAILABILITY

        if (vehicles[i].current_fuel <
            fuelNeeded)
            continue;

        // FIND DRIVER

        int driverIndex = -1;

        for (int d = 0; d < totalDrivers; d++)
        {
            if (drivers[d].driver_id ==
                vehicles[i].assigned_driver_id)
            {
                driverIndex = d;
                break;
            }
        }

        if (driverIndex == -1)
            continue;

        // DRIVER WORK LIMIT

        if (drivers[driverIndex]
            .hours_worked_today >=
            drivers[driverIndex]
            .max_daily_hours)
            continue;

        // HAZMAT LICENSE CHECK

        if (bins[emergencyBinIndex]
            .waste_type == HAZARDOUS &&
            !drivers[driverIndex]
            .has_hazmat_license)
            continue;

        // SELECT NEAREST VEHICLE

        if (distance < minimumDistance)
        {
            minimumDistance = distance;
            selectedVehicle = i;
        }
    }

    // ───────────────────────────────────────
    // NO SUITABLE VEHICLE
    // ───────────────────────────────────────

    if (selectedVehicle == -1)
    {
        printf("\nNo suitable emergency vehicle available.\n");

        printf("Emergency request could not be serviced");
        printf(" within operational constraints.\n");

        return;
    }

    // ───────────────────────────────────────
    // EMERGENCY ASSIGNMENT
    // ───────────────────────────────────────

    int totalDistance =
    minimumDistance * 2;

    float fuelUsed =
    (totalDistance / 10.0f) *
    vehicles[selectedVehicle]
    .fuel_consumption_rate;

    // UPDATE VEHICLE FUEL

    vehicles[selectedVehicle]
    .current_fuel -= fuelUsed;

    // UPDATE VEHICLE LOCATION

    vehicles[selectedVehicle]
    .current_x =
    bins[emergencyBinIndex].x;

    vehicles[selectedVehicle]
    .current_y =
    bins[emergencyBinIndex].y;

    // UPDATE VEHICLE ROUTE

    if (vehicles[selectedVehicle]
        .route_bin_count < 20)
    {
        vehicles[selectedVehicle]
        .route_bin_ids[
            vehicles[selectedVehicle]
            .route_bin_count
        ] =
        bins[emergencyBinIndex]
        .bin_id;

        vehicles[selectedVehicle]
        .route_bin_count++;
    }

    // VEHICLE TEMPORARILY BUSY

    vehicles[selectedVehicle]
    .is_available = false;

    // MARK BIN COLLECTED

    bins[emergencyBinIndex]
    .collected_today = true;

    bins[emergencyBinIndex]
    .fill_level = 0.0f;

    bins[emergencyBinIndex]
    .wpi =
    computeWPI(
        bins[emergencyBinIndex]
        .fill_level,

        bins[emergencyBinIndex]
        .waste_type
    );

    bins[emergencyBinIndex]
    .priority = PRI_LOW;

    // UPDATE DRIVER HOURS

    for (int d = 0; d < totalDrivers; d++)
    {
        if (drivers[d].driver_id ==
            vehicles[selectedVehicle]
            .assigned_driver_id)
        {
            drivers[d]
            .hours_worked_today += 2.0f;

            break;
        }
    }

    // SAVE ROUTE LOG

    FILE *routeFP = fopen(ROUTES_CSV, "a");

    if (routeFP)
    {
        if (isCsvEmpty(ROUTES_CSV))
        {
            fprintf(routeFP,
            "route_id,vehicle_id,driver_id,zone,total_distance,total_fuel,bins_collected\n");
        }

        fprintf(routeFP,
                "%d,%d,%d,%d,%d,%.2f,%d\n",

                generateRouteID(),

                vehicles[selectedVehicle]
                .vehicle_id,

                vehicles[selectedVehicle]
                .assigned_driver_id,

                bins[emergencyBinIndex]
                .zone,

                totalDistance,

                fuelUsed,

                1);

        fclose(routeFP);
    }

    // SAVE UPDATED DATABASES

    saveBinsToCSV(totalBins);
    saveVehiclesToCSV(totalVehicles);
    saveDriversToCSV(totalDrivers);

    // ───────────────────────────────────────
    // FINAL OUTPUT
    // ───────────────────────────────────────

    printf("\nEMERGENCY COLLECTION COMPLETED\n");

    printf("Assigned Vehicle : %d\n",
           vehicles[selectedVehicle]
           .vehicle_id);

    printf("Driver ID        : %d\n",
           vehicles[selectedVehicle]
           .assigned_driver_id);

    printf("Distance Covered : %d km\n",
           totalDistance);

    printf("Fuel Used        : %.2f liters\n",
           fuelUsed);

    printf("Vehicle Position : (%d, %d)\n",

           vehicles[selectedVehicle]
           .current_x,

           vehicles[selectedVehicle]
           .current_y);

    printf("Emergency bin collected successfully.\n");

    printf("\nNormal schedules dynamically adjusted.\n");
}

// ══════════════════════════════════════════════════════════════════════════════
//  USER MODULE
// ══════════════════════════════════════════════════════════════════════════════
int isValidZone(int zone) {
    return (zone >= 1 && zone <= MAX_ZONE);
}

void saveBinsToCSV(int total) {
    FILE *fp = fopen(BINS_CSV, "w");

    if (!fp) {
        printf("Error saving bins!\n");
        return;
    }

    fprintf(fp,
    "bin_id,zone,waste_type,fill_level,capacity,wpi,priority,x,y,last_collection,collected_today\n");

    for (int i = 0; i < total; i++) {

        fprintf(fp,
        "%lld,%d,%s,%.2f,%.2f,%.2f,%d,%d,%d,%s,%d\n",

        bins[i].bin_id,
        bins[i].zone,
        getWasteTypeString(bins[i].waste_type),
        bins[i].fill_level,
        bins[i].capacity,
        bins[i].wpi,
        bins[i].priority,
        bins[i].x,
        bins[i].y,
        bins[i].last_collection,
        (int)bins[i].collected_today
        );
    }

    fclose(fp);
}

void viewBinsByZone() {
    int zone;

    printf("Enter your zone: ");
    if (scanf("%d", &zone) != 1) {
        while (getchar() != '\n');
        printf("Invalid input!\n");
        return;
    }

    if (!isValidZone(zone)) {
        printf("Invalid zone!\n");
        return;
    }

    int total = loadBinsFromCSV();
    if (total == 0) {
        printf("No bin data available.\n");
        return;
    }

    printf("\n--- Bins in Zone %d ---\n", zone);

    int found = 0;

    for (int i = 0; i < total; i++) {
        if (bins[i].zone == zone) {
            printf("ID: %lld | Type: %-10s | Fill: %5.1f%% | WPI: %6.2f\n",
                   bins[i].bin_id,
                   getWasteTypeString(bins[i].waste_type),
                   bins[i].fill_level,
                   bins[i].wpi);
            found = 1;
        }
    }

    if (!found) {
        printf("No bins found in your zone.\n");
    }
}
void toUpperText(char text[]) {
    for (int i = 0; text[i] != '\0'; i++) {
        text[i] = (char)toupper((unsigned char)text[i]);
    }
}

bool containsKeyword(const char *text, const char *keyword) {
    return strstr(text, keyword) != NULL;
}

int classifyWasteByName(const char *originalItem) {
    char item[100];

    strncpy(item, originalItem, sizeof(item) - 1);
    item[sizeof(item) - 1] = '\0';
    toUpperText(item);

    if (containsKeyword(item, "BATTERY") ||
        containsKeyword(item, "CHEMICAL") ||
        containsKeyword(item, "PAINT") ||
        containsKeyword(item, "MEDICINE") ||
        containsKeyword(item, "MEDICAL") ||
        containsKeyword(item, "SYRINGE") ||
        containsKeyword(item, "E-WASTE") ||
        containsKeyword(item, "EWASTE") ||
        containsKeyword(item, "ELECTRONIC") ||
        containsKeyword(item, "BULB")) {
        return HAZARDOUS;
    }

    if (containsKeyword(item, "FOOD") ||
        containsKeyword(item, "VEGETABLE") ||
        containsKeyword(item, "FRUIT") ||
        containsKeyword(item, "PEEL") ||
        containsKeyword(item, "LEFTOVER") ||
        containsKeyword(item, "TEA") ||
        containsKeyword(item, "COFFEE") ||
        containsKeyword(item, "LEAF") ||
        containsKeyword(item, "LEAVES") ||
        containsKeyword(item, "ORGANIC")) {
        return WET;
    }

    if (containsKeyword(item, "PAPER") ||
        containsKeyword(item, "CARDBOARD") ||
        containsKeyword(item, "PLASTIC") ||
        containsKeyword(item, "BOTTLE") ||
        containsKeyword(item, "CAN") ||
        containsKeyword(item, "METAL") ||
        containsKeyword(item, "GLASS") ||
        containsKeyword(item, "CLOTH") ||
        containsKeyword(item, "WOOD")) {
        return DRY;
    }

    if (containsKeyword(item, "SOILED") ||
        containsKeyword(item, "CONTAMINATED") ||
        containsKeyword(item, "DIRTY") ||
        containsKeyword(item, "WRAPPER") ||
        containsKeyword(item, "PACKAGING") ||
        containsKeyword(item, "SANITARY")) {
        return MIXED;
    }

    return MIXED;
}
void throwWaste() {
    int zone, type, confirm;
    float waste;
    char itemName[100];

    printf("\n========================================");
    printf("\n          THROW WASTE MODULE            ");
    printf("\n========================================\n");

    printf("Enter waste item name: ");
    scanf(" %99[^\n]", itemName);

    type = classifyWasteByName(itemName);

    printf("\nWaste Item      : %s\n", itemName);
    printf("Classification  : %s\n", getWasteTypeString(type));
    printf("Recommended Bin : %s Bin\n", getWasteTypeString(type));

    printf("\nEnter your zone: ");
    if (scanf("%d", &zone) != 1) {
        while (getchar() != '\n');
        printf("Invalid input!\n");
        return;
    }

    if (!isValidZone(zone)) {
        printf("Invalid zone!\n");
        return;
    }

    int total = loadBinsFromCSV();
    if (total == 0) {
        printf("No bins available.\n");
        return;
    }

    int found = 0;

    printf("\nAvailable %s bins in Zone %d:\n",
           getWasteTypeString(type), zone);

    for (int i = 0; i < total; i++) {
        if (bins[i].zone == zone &&
            bins[i].waste_type == type &&
            bins[i].fill_level < 100.0f) {

            printf("ID: %lld | Fill: %.1f%% | Free: %.1f%% | WPI: %.2f\n",
                   bins[i].bin_id,
                   bins[i].fill_level,
                   100.0f - bins[i].fill_level,
                   bins[i].wpi);

            found = 1;
        }
    }

    if (!found) {
        printf("No suitable %s bin with available capacity found in this zone!\n",
               getWasteTypeString(type));
        return;
    }

    printf("\nDid you throw the waste in one of these bins?");
    printf("\n1. Yes");
    printf("\n2. No");
    printf("\nChoice: ");

    if (scanf("%d", &confirm) != 1) {
        while (getchar() != '\n');
        printf("Invalid input!\n");
        return;
    }

    if (confirm != 1) {
        printf("No bin updated. Returning to menu.\n");
        return;
    }

    long long int id;
    printf("Enter Bin ID to be used: ");
    if (scanf("%lld", &id) != 1) {
        while (getchar() != '\n');
        printf("Invalid Bin ID!\n");
        return;
    }

    printf("Enter waste amount to add in fill percentage: ");
    if (scanf("%f", &waste) != 1 || waste <= 0) {
        while (getchar() != '\n');
        printf("Invalid waste amount!\n");
        return;
    }

    for (int i = 0; i < total; i++) {
        if (bins[i].bin_id == id) {

            if (bins[i].zone != zone || bins[i].waste_type != type) {
                printf("Wrong bin selected! Please choose from the displayed list.\n");
                return;
            }

            if (bins[i].fill_level >= 100.0f) {
                printf("This bin is already full!\n");
                return;
            }

            if (bins[i].fill_level + waste <= 100.0f) {
                float oldFill = bins[i].fill_level;

                bins[i].fill_level += waste;
                bins[i].wpi = computeWPI(bins[i].fill_level, bins[i].waste_type);

                saveBinsToCSV(total);

                printf("\nWaste added successfully!\n");
                printf("Bin ID   : %lld\n", bins[i].bin_id);
                printf("Old Fill : %.1f%%\n", oldFill);
                printf("New Fill : %.1f%%\n", bins[i].fill_level);
                printf("New WPI  : %.2f\n", bins[i].wpi);

            } else {
                printf("Not enough space in this bin!\n");
                printf("Available space: %.1f%%\n", 100.0f - bins[i].fill_level);
            }

            return;
        }
    }

    printf("Invalid Bin ID!\n");
}

// ═════════════════════════════════════════════════════════════════════
// RAISE COMPLAINT + ESCALATION SYSTEM
// ═════════════════════════════════════════════════════════════════════

void raiseComplaint() {

    long long int id;

    int total = loadBinsFromCSV();

    if (total == 0)
        return;

    printf("Enter Bin ID to raise complaint: ");
    scanf("%lld", &id);

    getchar();

    for (int i = 0; i < total; i++) {

        if (bins[i].bin_id == id) {

            FILE *fp = fopen(COMPLAINTS_CSV, "a");

            if (!fp) {

                printf("Cannot open complaints file.\n");
                return;
            }

            if (isCsvEmpty(COMPLAINTS_CSV)) {

                fprintf(fp,
                "complaint_id,bin_id,zone,username,complaint_text,"
                "escalation_level,emergency_triggered,status\n");
            }

            char complaintText[150];

            printf("\nDescribe your complaint:\n");
            fgets(complaintText,
                  sizeof(complaintText),
                  stdin);

            complaintText[
                strcspn(complaintText, "\n")
            ] = '\0';

            int complaintID =
            generateComplaintID();

            int totalComplaints =
            countComplaintsForBin(id);

            int escalation = 1;

            bool triggerEmergency = false;

            // ===================================================
            // ESCALATION RULES
            // ===================================================

            if (totalComplaints >= 2)
                escalation = 2;

            if (totalComplaints >= 4)
                escalation = 3;

            // ===================================================
            // OVERFLOW EMERGENCY
            // ===================================================

            if (bins[i].fill_level >= 90)
                triggerEmergency = true;

            // ===================================================
            // HAZARDOUS WASTE EMERGENCY
            // ===================================================

            if (bins[i].waste_type == HAZARDOUS &&
                bins[i].fill_level >= 70) {

                triggerEmergency = true;
            }

            // ===================================================
            // MULTIPLE COMPLAINTS EMERGENCY
            // ===================================================

            if (totalComplaints >= 3)
                triggerEmergency = true;

            // ===================================================
            // SAVE COMPLAINT
            // ===================================================

            fprintf(fp,
            "%d,%lld,%d,%s,%s,%d,%d,%s\n",

            complaintID,
            bins[i].bin_id,
            bins[i].zone,
            "NETIZEN",
            complaintText,
            escalation,
            (int)triggerEmergency,
            "PENDING");

            fclose(fp);

            printf("\n====================================");
            printf("\n COMPLAINT REGISTERED");
            printf("\n====================================");

            printf("\nComplaint ID     : %d",
                   complaintID);

            printf("\nEscalation Level : %d",
                   escalation);

            printf("\nStatus           : PENDING\n");

            // ===================================================
            // AUTO EMERGENCY HANDLING
            // ===================================================

            if (triggerEmergency) {

                printf("\nEMERGENCY CONDITION DETECTED.\n");

                emergencyHandler(id);
            }

            return;
        }
    }

    printf("Invalid Bin ID.\n");
}

void viewComplaints() {

    FILE *fp = fopen(COMPLAINTS_CSV, "r");

    if (!fp) {

        printf("No complaints found.\n");
        return;
    }

    char line[512];

    fgets(line, sizeof(line), fp);

    printf("\n=========== COMPLAINTS ===========\n");

    while (fgets(line, sizeof(line), fp)) {

        Complaint c;

        sscanf(line,
               "%d,%lld,%d,%[^,],%[^,],%d,%d,%s",

               &c.complaint_id,
               &c.bin_id,
               &c.zone,
               c.username,
               c.complaint_text,
               &c.escalation_level,
               (int *)&c.emergency_triggered,
               c.status);

        printf("\nComplaint ID : %d\n",
               c.complaint_id);

        printf("Bin ID       : %lld\n",
               c.bin_id);

        printf("Zone         : %d\n",
               c.zone);

        printf("User         : %s\n",
               c.username);

        printf("Issue        : %s\n",
               c.complaint_text);

        printf("Escalation   : Level %d\n",
               c.escalation_level);

        printf("Emergency    : %s\n",
               c.emergency_triggered ?
               "YES" : "NO");

        printf("Status       : %s\n",
               c.status);
    }

    fclose(fp);
}

// ═════════════════════════════════════════════════════════════════════
// RESOLVE COMPLAINT MODULE
// ═════════════════════════════════════════════════════════════════════

void resolveComplaint() {

    FILE *fp = fopen(COMPLAINTS_CSV, "r");

    if (!fp) {

        printf("No complaints database found.\n");
        return;
    }

    char line[512];

    Complaint complaintList[100];

    int complaintCount = 0;

    fgets(line, sizeof(line), fp);

    printf("\n============= PENDING COMPLAINTS =============\n");

    while (fgets(line, sizeof(line), fp)) {

        Complaint c;

        sscanf(line,
               "%d,%lld,%d,%[^,],%[^,],%d,%d,%s",

               &c.complaint_id,
               &c.bin_id,
               &c.zone,
               c.username,
               c.complaint_text,
               &c.escalation_level,
               (int *)&c.emergency_triggered,
               c.status);

        complaintList[complaintCount] = c;

        if (strcmp(c.status, "PENDING") == 0) {

            printf("\nComplaint ID : %d\n",
                   c.complaint_id);

            printf("Bin ID       : %lld\n",
                   c.bin_id);

            printf("Zone         : %d\n",
                   c.zone);

            printf("Issue        : %s\n",
                   c.complaint_text);

            printf("Escalation   : Level %d\n",
                   c.escalation_level);

            printf("Emergency    : %s\n",
                   c.emergency_triggered ?
                   "YES" : "NO");
        }

        complaintCount++;
    }

    fclose(fp);

    if (complaintCount == 0) {

        printf("No complaints available.\n");
        return;
    }

    int selectedComplaintID;

    printf("\nEnter Complaint ID To Resolve: ");
    while (getchar() != '\n');
    scanf("%d", &selectedComplaintID);

    int complaintIndex = -1;

    for (int i = 0; i < complaintCount; i++) {

        if (complaintList[i].complaint_id ==
            selectedComplaintID) {

            complaintIndex = i;
            break;
        }
    }

    if (complaintIndex == -1) {

        printf("Complaint not found.\n");
        return;
    }

    int totalBins = loadBinsFromCSV();

    int totalVehicles = loadVehiclesFromCSV();

    int totalDrivers = loadDriversFromCSV();

    int binIndex = -1;

    // FIND BIN

    for (int i = 0; i < totalBins; i++) {

        if (bins[i].bin_id ==
            complaintList[complaintIndex].bin_id) {

            binIndex = i;
            break;
        }
    }

    if (binIndex == -1) {

        printf("Associated bin not found.\n");
        return;
    }

    // FIND NEAREST SUITABLE VEHICLE

    int bestVehicle = -1;

    int minimumDistance = 999999;

    for (int i = 0; i < totalVehicles; i++) {

        // VEHICLE CHECKS

        if (vehicles[i].under_maintenance)
            continue;

        if (vehicles[i].assigned_driver_id == 0)
            continue;

        if (vehicles[i].current_fuel <= 0)
            continue;

        int distance =
        calculateManhattanDistance(
            HUB_X,
            HUB_Y,
            bins[binIndex].x,
            bins[binIndex].y
        );

        float fuelNeeded =
        ((distance * 2) / 10.0f) *
        vehicles[i].fuel_consumption_rate;

        if (vehicles[i].current_fuel <
            fuelNeeded)
            continue;

        // DRIVER CHECKS

        int driverIndex = -1;

        for (int d = 0; d < totalDrivers; d++) {

            if (drivers[d].driver_id ==
                vehicles[i].assigned_driver_id) {

                driverIndex = d;
                break;
            }
        }

        if (driverIndex == -1)
            continue;

        if (drivers[driverIndex]
            .hours_worked_today >=
            drivers[driverIndex]
            .max_daily_hours)
            continue;

        // HAZMAT CHECK

        if (bins[binIndex].waste_type ==
            HAZARDOUS &&
            !drivers[driverIndex]
            .has_hazmat_license)
            continue;

        // NEAREST VEHICLE

        if (distance < minimumDistance) {

            minimumDistance = distance;

            bestVehicle = i;
        }
    }

    if (bestVehicle == -1) {

        printf("\nNo suitable vehicle available.\n");
        return;
    }

    // ASSIGN COLLECTION

    int totalDistance =
    minimumDistance * 2;

    float fuelUsed =
    (totalDistance / 10.0f) *
    vehicles[bestVehicle]
    .fuel_consumption_rate;

    vehicles[bestVehicle]
    .current_fuel -= fuelUsed;

    bins[binIndex]
    .collected_today = true;

    // UPDATE DRIVER HOURS

    for (int d = 0; d < totalDrivers; d++) {

        if (drivers[d].driver_id ==
            vehicles[bestVehicle]
            .assigned_driver_id) {

            drivers[d]
            .hours_worked_today += 1.5f;

            break;
        }
    }

    // SAVE ROUTE

    FILE *routeFP = fopen(ROUTES_CSV, "a");

    if (routeFP) {

        if (isCsvEmpty(ROUTES_CSV)) {

            fprintf(routeFP,
            "route_id,vehicle_id,driver_id,zone,total_distance,total_fuel,bins_collected\n");
        }

        fprintf(routeFP,
                "%d,%d,%d,%d,%d,%.2f,%d\n",

                generateRouteID(),

                vehicles[bestVehicle]
                .vehicle_id,

                vehicles[bestVehicle]
                .assigned_driver_id,

                bins[binIndex]
                .zone,

                totalDistance,

                fuelUsed,

                1);

        fclose(routeFP);
    }

    // MARK COMPLAINT RESOLVED

    strcpy(
        complaintList[complaintIndex].status,
        "RESOLVED"
    );

    // REWRITE COMPLAINT FILE

    fp = fopen(COMPLAINTS_CSV, "w");

    fprintf(fp,
    "complaint_id,bin_id,zone,username,complaint_text,"
    "escalation_level,emergency_triggered,status\n");

    for (int i = 0; i < complaintCount; i++) {

        fprintf(fp,
                "%d,%lld,%d,%s,%s,%d,%d,%s\n",

                complaintList[i].complaint_id,

                complaintList[i].bin_id,

                complaintList[i].zone,

                complaintList[i].username,

                complaintList[i].complaint_text,

                complaintList[i].escalation_level,

                (int)complaintList[i]
                .emergency_triggered,

                complaintList[i].status);
    }

    fclose(fp);

    // SAVE DATABASES

    saveBinsToCSV(totalBins);

    saveVehiclesToCSV(totalVehicles);

    saveDriversToCSV(totalDrivers);

    printf("\n========================================");
    printf("\n      COMPLAINT RESOLVED SUCCESSFULLY   ");
    printf("\n========================================\n");

    printf("Complaint ID   : %d\n",
           selectedComplaintID);

    printf("Assigned Vehicle : %d\n",
           vehicles[bestVehicle]
           .vehicle_id);

    printf("Driver ID        : %d\n",
           vehicles[bestVehicle]
           .assigned_driver_id);

    printf("Distance Travelled : %d km\n",
           totalDistance);

    printf("Fuel Used : %.2f liters\n",
           fuelUsed);

    printf("Bin Collected Successfully.\n");
}

// ═════════════════════════════════════════════════════════════════════
// COUNT COMPLAINTS FOR SAME BIN
// ═════════════════════════════════════════════════════════════════════

int countComplaintsForBin(long long int binID) {

    FILE *fp = fopen(COMPLAINTS_CSV, "r");

    if (!fp)
        return 0;

    char line[512];

    int count = 0;

    fgets(line, sizeof(line), fp);

    while (fgets(line, sizeof(line), fp)) {

        Complaint c;

        sscanf(line,
               "%d,%lld,%d,%[^,],%[^,],%d,%d,%s",

               &c.complaint_id,
               &c.bin_id,
               &c.zone,
               c.username,
               c.complaint_text,
               &c.escalation_level,
               (int *)&c.emergency_triggered,
               c.status);

        if (c.bin_id == binID &&
            strcmp(c.status, "PENDING") == 0) {

            count++;
        }
    }

    fclose(fp);

    return count;
}

void suggestBestBin() {

    int zone, type;

    printf("Enter your zone: ");
    scanf("%d", &zone);

    printf("\nWaste Types:");
    printf("\n1. Dry");
    printf("\n2. Wet");
    printf("\n3. Mixed");
    printf("\n4. Hazardous");

    printf("\nEnter waste type (1-4): ");
    scanf("%d", &type);

    int total = loadBinsFromCSV();

    if (total == 0) {
        printf("No bins available.\n");
        return;
    }

    int best = -1;
    float minFill = 101;

    // =========================================================
    // SEARCH FOR SAME WASTE TYPE BIN
    // =========================================================

    for (int i = 0; i < total; i++) {

        if (bins[i].zone == zone &&
            bins[i].waste_type == type) {

            if (bins[i].fill_level < minFill) {

                minFill = bins[i].fill_level;
                best = i;
            }
        }
    }

    // =========================================================
    // EXACT MATCH FOUND
    // =========================================================

    if (best != -1) {

        printf("\n====================================");
        printf("\n BEST MATCHING BIN FOUND");
        printf("\n====================================");

        printf("\nBin ID      : %lld",
               bins[best].bin_id);

        printf("\nZone        : %d",
               bins[best].zone);

        printf("\nFill Level  : %.2f%%",
               bins[best].fill_level);

        printf("\nPriority    : %d",
               bins[best].priority);

        printf("\nCoordinates : (%d,%d)\n",
               bins[best].x,
               bins[best].y);

        return;
    }

    // =========================================================
    // NO SAME TYPE BIN -> SHOW OTHER BINS
    // =========================================================

    printf("\nNo matching waste-type bin found.");
    printf("\nShowing other available bins in your zone:\n");

    int foundOther = 0;

    for (int i = 0; i < total; i++) {

        if (bins[i].zone == zone) {

            foundOther = 1;

            printf("\n----------------------------------");

            printf("\nBin ID      : %lld",
                   bins[i].bin_id);

            printf("\nWaste Type  : %d",
                   bins[i].waste_type);

            printf("\nFill Level  : %.2f%%",
                   bins[i].fill_level);

            printf("\nPriority    : %d",
                   bins[i].wpi);

            printf("\nCoordinates : (%d,%d)\n",
                   bins[i].x,
                   bins[i].y);
        }
    }

    // =========================================================
    // NO BINS IN LOCALITY
    // =========================================================

    if (!foundOther) {

        printf("\nSorry :( No bins available in your locality.");
        printf("\nYou may register a complaint.\n");
    }
}

// ═════════════════════════════════════════════════════════════════════
// CALCULATE FUEL COST
// ═════════════════════════════════════════════════════════════════════

float calculateFuelCost(float fuelUsed) {

    float totalCost =
    fuelUsed * FUEL_PRICE_PER_LITER;

    return totalCost;
}

// ═════════════════════════════════════════════════════════════════════
// GENERATE OPERATIONAL REPORT
// ═════════════════════════════════════════════════════════════════════

void generateReport() {

    FILE *routeFP = fopen(ROUTES_CSV, "r");

    if (!routeFP) {
        printf("No route records available.\n");
        return;
    }

    int totalRoutes = 0;
    float totalDistance = 0;
    float totalFuelUsed = 0;
    int totalBinsCollected = 0;

    char line[512];

    fgets(line, sizeof(line), routeFP);

    while (fgets(line, sizeof(line), routeFP)) {

        Route r;

        sscanf(line,
               "%d,%d,%d,%d,%f,%f,%d",

               &r.route_id,
               &r.vehicle_id,
               &r.driver_id,
               &r.zone,
               &r.total_distance,
               &r.total_fuel_used,
               &r.bins_collected);

        totalRoutes++;

        totalDistance += r.total_distance;

        totalFuelUsed += r.total_fuel_used;

        totalBinsCollected += r.bins_collected;
    }

    fclose(routeFP);

    // ── Fuel Cost ─────────────────────────────────────

    float totalFuelCost =
    calculateFuelCost(totalFuelUsed);

    // ── Driver Salary Calculation ────────────────────

    int totalDrivers = loadDriversFromCSV();

    float totalSalary = 0;

    for (int i = 0; i < totalDrivers; i++) {

        totalSalary +=
        drivers[i].hours_worked_today *
        DRIVER_SALARY_PER_HOUR;
    }

    // ── Maintenance Cost ─────────────────────────────

    int totalVehicles = loadVehiclesFromCSV();

    float maintenanceCost = 0;

    for (int i = 0; i < totalVehicles; i++) {

        if (vehicles[i].under_maintenance) {

            maintenanceCost +=
            MAINTENANCE_COST_PER_VEHICLE;
        }
    }

    // ── Emergency Adjustments ────────────────────────

    FILE *emergencyFP = fopen(EMERGENCY_CSV, "r");

    int emergencyCount = 0;

    if (emergencyFP) {

        while (fgets(line, sizeof(line), emergencyFP)) {

            emergencyCount++;
        }

        fclose(emergencyFP);

        // reduce one because header may exist
        if (emergencyCount > 0)
            emergencyCount--;
    }

    float emergencyCost =
    emergencyCount * 5000.0f;

    // ── Recycling Efficiency ─────────────────────────

    int totalBins = loadBinsFromCSV();

    int recyclableBins = 0;

    for (int i = 0; i < totalBins; i++) {

        if (bins[i].waste_type == DRY ||
            bins[i].waste_type == WET) {

            recyclableBins++;
        }
    }

    float recyclingEfficiency = 0;

    if (totalBins > 0) {

        recyclingEfficiency =
        ((float)recyclableBins /
         totalBins) * 100.0f;
    }

    // ── Final Operational Cost ───────────────────────

    float totalOperationalCost =
    totalFuelCost +
    totalSalary +
    maintenanceCost +
    emergencyCost;

    // ── REPORT OUTPUT ────────────────────────────────

    printf("\n====================================================");
    printf("\n        SENTRABIN OPERATIONAL REPORT");
    printf("\n====================================================\n");

    printf("Total Routes Executed        : %d\n",
           totalRoutes);

    printf("Total Distance Covered       : %.2f km\n",
           totalDistance);

    printf("Total Fuel Used              : %.2f liters\n",
           totalFuelUsed);

    printf("Fuel Cost                    : Rs. %.2f\n",
           totalFuelCost);

    printf("Driver Salary Expense        : Rs. %.2f\n",
           totalSalary);

    printf("Maintenance Cost             : Rs. %.2f\n",
           maintenanceCost);

    printf("Emergency Handling Cost      : Rs. %.2f\n",
           emergencyCost);

    printf("Total Operational Cost       : Rs. %.2f\n",
           totalOperationalCost);

    printf("Bins Collected               : %d\n",
           totalBinsCollected);

    printf("Recycling Efficiency         : %.2f%%\n",
           recyclingEfficiency);

    // ── Operational Intelligence ─────────────────────

    printf("\n================ ANALYSIS ================\n");

    if (recyclingEfficiency >= RECYCLING_EFFICIENCY_TARGET)
        printf("Recycling Efficiency Status : GOOD\n");
    else
        printf("Recycling Efficiency Status : NEEDS IMPROVEMENT\n");

    if (emergencyCount > 5)
        printf("Emergency Frequency         : HIGH\n");
    else
        printf("Emergency Frequency         : NORMAL\n");

    if (maintenanceCost > 10000)
        printf("Fleet Maintenance Status    : CRITICAL\n");
    else
        printf("Fleet Maintenance Status    : STABLE\n");

    if (totalFuelUsed > 500)
        printf("Fuel Consumption            : HIGH\n");
    else
        printf("Fuel Consumption            : OPTIMAL\n");

    printf("====================================================\n");
}


// ══════════════════════════════════════════════════════════════════════════════
//  MENU MODULE
// ══════════════════════════════════════════════════════════════════════════════

void adminMenu(const char* adminName) {

    int choice;
    int sub;

    while (1) {

        int totalBins = loadBinsFromCSV();

        printf("\n====================================================");
        printf("\n        SENTRABIN OS ADMIN PANEL");
        printf("\n====================================================\n");

        printf("Operator : %s\n", adminName);
        printf("Total Bins : %d\n", totalBins);

        printf("\n1. BIN MANAGEMENT\n");
        printf("2. VEHICLE MANAGEMENT\n");
        printf("3. DRIVER MANAGEMENT\n");
        printf("4. ROUTE MANAGEMENT\n");
        printf("5. REPORT & ANALYSIS\n");
        printf("0. EXIT\n");

        printf("Choice: ");
        scanf("%d", &choice);

        switch (choice) {

            case 1:

                printf("\n1. Create Bin\n");
                printf("2. Identify Critical Bins\n");
                printf("Choice: ");
                scanf("%d", &sub);

                if (sub == 1)
                    createBin();
                else if (sub == 2)
                    identifyCriticalBins();
                else
                    printf("Invalid choice!");

                break;

            case 2:

                printf("\n1. Create Vehicle\n");
                printf("2. View Vehicles\n");
                printf("Choice: ");
                scanf("%d", &sub);

                if (sub == 1)
                    createVehicle();
                else if (sub == 2)
                    viewVehicles();
                else
                    printf("Invalid choice!");
                break;

            case 3:

                printf("\n1. Create Driver\n");
                printf("2. View Drivers\n");
                printf("3. Assign Drivers To Vehicles\n");
                printf("4. Generate Distance Matrix\n");
                printf("5. Raise Emergency\n");
                printf("Choice: ");

                scanf("%d", &sub);

                if (sub == 1)
                    createDriver();

                else if (sub == 2)
                    viewDrivers();

                else if (sub == 3)
                    assignDriversToVehicles();

                else if (sub == 4)
                    generateDistanceMatrix();

                else if (sub == 5) {

                    long long int emergencyBinID;

                    printf("Enter Emergency Bin ID: ");
                    scanf("%lld", &emergencyBinID);

                    emergencyHandler(emergencyBinID);
                }

                else
                    printf("Invalid choice!");
                    break;

            case 4:

                printf("\n1. Run Route Optimization\n");
                printf("2. View Complaints\n");
                printf("3. Resolve Complaints\n");
                printf("Choice: ");
                scanf("%d", &sub);

                if (sub == 1)
                    runRouteOptimization();
                else if (sub == 2)
                    viewComplaints();
                else if (sub == 3)
                    resolveComplaint();
                else
                    printf("Invalid choice!");
                break;

            case 5:

                printf("\n1. Generate Operational Report\n");
                printf("Choice: ");
                scanf("%d", &sub);

                if (sub == 1)
                generateReport();
                else
                    printf("Invalid choice!");
                break;

            case 0:
                return;

            default:
                printf("Invalid Choice!\n");
        }
    }
}

void netizenMenu(const char *username) {
    int choice;
    while (1) {
        printf("\n--- WELCOME %s (USER MODE) ---", username);
        printf("\n1. View Bins by Zone\n2. Throw Waste\n3. Raise Complaint\n4. Suggest Best Bin\n0. Logout\nChoice: ");
        if (scanf("%d", &choice) != 1) {
            while(getchar() != '\n'); continue;
        }
        switch (choice) {
            case 1: viewBinsByZone(); break;
            case 2: throwWaste(); break;
            case 3: raiseComplaint(); break;
            case 4: suggestBestBin(); break;
            case 0: return;
            default: printf("Invalid choice!\n");
        }

    }
}


// ══════════════════════════════════════════════════════════════════════════════
//      USER AUTHENTICATION MODULE
// ══════════════════════════════════════════════════════════════════════════════

    const char* userAUTH(char* outUsername) {
        char pass[32];


        printf("==========================================\n");
        printf("        SENTRABIN OS - SECURE LOGIN       \n");
        printf("==========================================\n");

        printf("\n  ENTER USERNAME: ");
        scanf("%39s", outUsername); // Store it in the buffer passed from main
        printf("  ENTER PASSCODE: ");
        scanf("%31s", pass);

        if (strcmp(pass, ADMIN_PASS) == 0) {
            printf("\n[AUTH] Admin privileges granted to: %s\n", outUsername);
            return "admin";
        }
        else if (strcmp(pass, NETIZEN_PASS) == 0) {
            printf("\n[AUTH] Netizen access granted to: %s\n", outUsername);
            return "netizen";
        }

        return "unauthorized";
    }

// ══════════════════════════════════════════════════════════════════════════════
//  MAIN FUNCTION
// ══════════════════════════════════════════════════════════════════════════════

    int main(){
        char username[32];
        const char* role = userAUTH(username);

        if (strcmp(role, "admin") == 0) {
            adminMenu(username);
        }
        else if (strcmp(role, "netizen") == 0) {
            netizenMenu(username);
        }
        else {
            printf("\n[ERROR] Access Denied. Closing SENTRABIN OS...\n");
            return 1;
        }

        printf("\nSession Ended. Goodbye!\n");
        return 0;
}
