#include <stdio.h>
//Bin Constants
#define MAX_BINS 100
#define BINS_CSV "bins.csv"
//Waste Types
#define DRY 

struct Bin{
    int bin_id;
    int zone;
    int waste_type;
    float fill_level;
    float capacity;
    
};