#ifndef __PID_H__
#define __PID_H__

#define LIMIT_MAX_MIN(x, max, min)	(((x) <= (min)) ? (min):(((x) >= (max)) ? (max) : (x)))

typedef struct PID{
		float SetPoint;			//设定目标值
		float ActualValue;
		
		float P;						//比例常数
		float I;						//积分常数
		float D;						//微分常数
		
		float LastError;		//前次误差
		float PreError;			//当前误差
		float SumError;			//积分误差
		float dError;
	
		float IMax;					//积分限制
		float OutMax;
		
		float POut;					//比例输出
		float IOut;					//积分输出
		float DOut;					//微分输出
	
		float Out;
}Pid_Typedef;

typedef struct {
	Pid_Typedef pos_pid;
	Pid_Typedef speed_pid;
}Pos_Speed_PID;

float PID_Calc(Pid_Typedef * P, float Setpoint,float ActualValue);
#endif
