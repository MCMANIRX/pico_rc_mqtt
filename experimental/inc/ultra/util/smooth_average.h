#define MAX_SAMPLE_COUNT 48
#define VALUE_THRESH 300
#define ROC_THRESH 150
#include <math.h>

enum pre_filter {

  RANGE,
  RANGE_AND_ROC

};

typedef struct {
    float points[MAX_SAMPLE_COUNT];
    float sum;
    float last;
    int samples;
    float roc_buf[2];
    float roc;

} smooth_average;

void init_smooth(smooth_average *inst, int _samples) {
    inst->sum = 0;
    inst->last = 0;
    inst->roc = 0;
    inst->samples = _samples;
}

float kalman(float U){
  static const float R = 40;
  static const float H = 1.00;
  static float Q = 10;
  static float P = 0;
  static float U_hat = 0;
  static float K = 0;
  K = P*H/(H*P*H+R);
  U_hat += + K*(U-H*U_hat);
  P = (1-K*H)*P+Q;
  return U_hat;
}

// Helper function for qsort (assuming floats)
int float_comp(const void* elem1, const void* elem2)
{
    if(*(const float*)elem1 < *(const float*)elem2)
        return -1;
    return *(const float*)elem1 > *(const float*)elem2;
}

void cycle_points(float *points,float point, int len) {
      // Shift existing points one position

  for (int i = len - 1; i > 0; i--) {
    points[i] = points[i - 1];
      points[0] = point;

  }
}

static int pre_filter_data(smooth_average *inst, float point, enum pre_filter pf)
{
  if (pf == RANGE)
  {
    if (point > VALUE_THRESH || point < 5)
      return 1;
  }
  else if (pf == RANGE_AND_ROC)
  {

    cycle_points(inst->roc_buf, point, 2);
    if (inst->roc == 0)
      inst->roc = point;
    else
      inst->roc = fabs(inst->roc_buf[0] - inst->roc_buf[1]);

    if (point > VALUE_THRESH || point < 5)
      return 1; //= point > AVG_THRESH ? AVG_THRESH : inst->last;

    if (inst->roc > ROC_THRESH && fabs(inst->last - point) > ROC_THRESH)
      return 1;
  }
  return 0;
}

float filter_average(smooth_average *inst, float point, enum pre_filter pf) {
    inst->sum = 0;

    if(pre_filter_data(inst,point,pf))return inst->last;


    for(int i = 1 ; i < inst->samples ; ++i){
        inst->points[i] = inst->points[i-1];
        inst->sum += i > 0 ? inst->points[i] : 0;
        inst->points[0]  = point;
    }
    inst->last = (inst->points[0]+inst->sum) / ((float)inst->samples);
   // printf("\navg is %.2f \n %.2f %.2f %.2f %.2f\n",inst->avg,inst->points[0],inst->points[1],inst->points[2],inst->points[3]);

    return inst->last;

}

float filter_average_kalman(smooth_average *inst, float point, enum pre_filter pf) {
    inst->sum = 0;

    if(pre_filter_data(inst,point,pf))return inst->last;


    for(int i = 1 ; i < inst->samples ; ++i){
        inst->points[i] = inst->points[i-1];
        inst->sum += i > 0 ? inst->points[i] : 0;
        inst->points[0]  = point;
    }
    inst->last = (inst->points[0]+inst->sum) / ((float)inst->samples);
   // printf("\navg is %.2f \n %.2f %.2f %.2f %.2f\n",inst->avg,inst->points[0],inst->points[1],inst->points[2],inst->points[3]);

    return kalman(inst->last);

}





float filter_median(smooth_average *inst, float point, enum pre_filter pf) {
  // No need to reset sum here, median doesn't use it

    if(pre_filter_data(inst,point,pf))return inst->last;


  // Shift existing points one position
 // for (int i = SAMPLE_COUNT - 1; i > 0; i--) {
  //  inst->points[i] = inst->points[i - 1];
 // }

  // Insert the new point at the beginning
  inst->points[0] = point;

  // Sort the points array for median calculation
  // (implementation depends on your chosen sorting library)
  qsort(inst->points, inst->samples, sizeof(float), float_comp);

  // Return the median value (middle element)
  //inst->last = kalman(inst->points[SAMPLE_COUNT / 2]);
  return inst->last ;
}

float filter_median_kalman(smooth_average *inst, float point,enum pre_filter pf) {
  // No need to reset sum here, median doesn't use it

    if(pre_filter_data(inst,point,pf))return inst->last;



    cycle_points(inst->points,point,inst->samples);

    //for (int i = inst->samples - 1; i > 0; i--) 
    //  inst->points[i] = inst->points[i - 1];
 

  // Sort the points array for median calculation
  // (implementation depends on your chosen sorting library)
  qsort(inst->points, inst->samples, sizeof(float), float_comp);

 /* for(int i = 0; i < inst->samples; ++i) //AI gave bad code, but forums never disappoint!
    printf("%.2f ",inst->points[i]);
    printf("\n");*/

  // Return the median value (middle element)
  inst->last = kalman(inst->points[inst->samples / 2]);
  return inst->last ;
}


float filter_kalman(smooth_average *inst, float point,enum pre_filter pf){

    if(pre_filter_data(inst,point,pf))return inst->last;


    inst->last = kalman(point); 
        return inst->last;
}



  // Insert the new point at the beginning


