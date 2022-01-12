
#include "AsyncThreadPool.h"

ATP_STAT sub1(PATP_DATA data)
{
	TRACE("--------------- %s func, data is (%s), with threadno(%d)\n", __func__, data->s ? data->s : "", data->threadNo);
	usleep(500000); // 0.5 second

	return stat_suspend;
}

ATP_STAT sub2(PATP_DATA data)
{
	TRACE("--------------- %s func, data is (%s), with threadno(%d)\n", __func__, data->s ? data->s : "", data->threadNo);
	sleep(1); // 1 second

	return stat_suspend;
}

ATP_STAT sub3(PATP_DATA data)
{
	TRACE("--------------- %s func, data is (%s), with threadno(%d)\n", __func__, data->s ? data->s : "", data->threadNo);
	sleep(2); // 2 second

	return stat_suspend;
}

ATP_STAT sub9(PATP_DATA data)
{
	TRACE("--------------- %s func, data is (%s), with threadno(%d)\n", __func__, data->s ? data->s : "", data->threadNo);
	sleep(90);

	return stat_suspend;
}

ATP_STAT lock1(PATP_DATA data)
{
	TRACE("111111111111111 %s func, wait   lock, with threadno(%d), lock queue count(%d)\n", __func__, data->threadNo, atp_getWorkLockCount());
	atp_worklock();
	TRACE("222222222222222 %s func, got    lock, with threadno(%d), lock queue count(%d)\n", __func__, data->threadNo, atp_getWorkLockCount());
	sleep(1);
	TRACE("333333333333333 %s func, finish job, with threadno(%d)\n", __func__, data->threadNo);
	atp_workunlock();
	// TRACE("444444444444444 %s func, after  unlock, with threadno(%d)\n", __func__, data->threadNo);

	return stat_suspend;
}

ATP_STAT lock2(PATP_DATA data)
{
	TRACE("111111111111111 %s func, wait   lock, with threadno(%d), lock queue count(%d)\n", __func__, data->threadNo, atp_getWorkLockCount());
	atp_worklock();
	TRACE("222222222222222 %s func, got    lock, with threadno(%d), lock queue count(%d)\n", __func__, data->threadNo, atp_getWorkLockCount());
	sleep(2);
	TRACE("333333333333333 %s func, finish job, with threadno(%d)\n", __func__, data->threadNo);
	atp_workunlock();
	// TRACE("444444444444444 %s func, after  unlock, with threadno(%d)\n", __func__, data->threadNo);

	return stat_suspend;
}



ATP_STAT test(PATP_DATA param)
{
	if (param->func) {
		return param->func(param);
	} else {
		TRACE(" %s func, message is (%s), with threadno(%d)\n", __FUNCTION__, param->s ? param->s : "", param->threadNo);
	}
	return stat_suspend;
}

ATP_STAT exit_func(PATP_DATA param)
{
	TRACE(" %s func, OK exit func check (%s), with threadno(%d)\n", __FUNCTION__, param && param->s ? param->s : "", param->threadNo);

	return stat_exited;
}



int main(int argc, char* argv[])
{
	TRACE("--- thread pool basic test ---\n");

	size_t data_size = 1024;
	PATP_DATA atpdata;
	int nIndx, next;
#if defined(DEBUGTRACE)
	PTHREADINFO pThread;
#endif
	ATP_END end = argc > 1 ? atoi(argv[1]) ? gracefully : force : gracefully;
	bool exit = argc > 2 ? atoi(argv[2]) ? true : false : false;

	atp_create(3, test);

	TRACE("--- timewait test 7 \n");
	sleep(7); // idle check

	srand(1);

	for (nIndx = 0; nIndx < 15; nIndx++) {

		// atpdata 는 쓰레딩 작업후에 자동으로 free 된다
		atpdata = atp_alloc(data_size);
		
		snprintf(atpdata->s, atpdata->s_len, "job seq (%d)", nIndx);

		next = nIndx % 3;

		switch (next) {
		case 0:
			atpdata->func = sub1;
			if (!atp_addQueue(atpdata)) {
				TRACE("++++++++++++++++++++ thread add real Queue successed %d \n", nIndx);
			} else {
				free(atpdata);
				TRACE("++++++++++++++++++++ thread add real Queue failed %d \n", nIndx);
			}
			break;
		case 1:
			atpdata->func = sub2;
			if (!atp_addQueue(atpdata)) {
				TRACE("++++++++++++++++++++ thread add real Queue successed %d \n", nIndx);
			}
			else {
				free(atpdata);
				TRACE("++++++++++++++++++++ thread add real Queue failed %d \n", nIndx);
			}
			break;
		default:
			atpdata->func = sub3;
			if (!atp_addQueue(atpdata,atp_normal)) 
				TRACE("++++++++++++++++++++ thread add normal Queue successed %d \n", nIndx);
			else {
				free(atpdata);
				TRACE("++++++++++++++++++++ thread add normal Queue failed %d \n", nIndx);
			}
		}
		// realtime 우선순위의 다 처리하면 중간중간 normal 우선순의를 실행할 수 있도록 시간 맞추기 힘들군...
		next = rand() % 600;
		usleep(next*1000); // 작업의뢰를 램덤한 간격으로 한다 ( 0 ~ 0.6초)
	}

	// 강제 킬 목적으로 sub9 할당
	atpdata = atp_alloc(data_size);
	snprintf(atpdata->s, atpdata->s_len, "wait process.. kill me (XXX)");
	atpdata->func = sub9;
	atp_addQueue(atpdata);


	//sleep(2);

	
	TRACE("--- thread pool terminate ---\n");
	TRACE("--- thread realtime queue_size(%d), normal queue_size(%d)\n", atp_getRealtimeQueueCount(), atp_getNormalQueueCount());
#if defined(DEBUGTRACE)
	TRACE("-------------------------------------------\n");
	pThread = atp_getThreadInfo();
	for (nIndx = 0; nIndx < atp_getThreadCount(); nIndx++) {
		TRACE("threadno=%d, realtime execute=%ju average elapsed=%ju, normal execute=%ju average elapsed=%ju\n"
			, pThread[nIndx].nThreadNo
			, pThread[nIndx].nRealtimeCount
			, atp_getAverageRealtimeWorkingtime(nIndx)	// 평균 작업소요 시간
			, pThread[nIndx].nNormalCount
			, atp_getAverageNormalWorkingtime(nIndx) // 평균 작업소요 시간
		);
	}
	TRACE("-------------------------------------------\n");
#endif


	TRACE("--- endcode(%s) exit_func(%s)\n", end == gracefully ? "gracefully" : "force", exit ? "true" : "fale");
	
	if (exit) {
#if 0
		atp_setfunc(stat_exit, exit_func, NULL, -1);
#else
		for (nIndx = 0; nIndx < atp_getThreadCount(); nIndx++) {
			atpdata = atp_alloc(32);
			sprintf(atpdata->s, "work no %d", nIndx);
			atp_setfunc(stat_exit, exit_func, atpdata, nIndx);
		}
#endif
	}

	atp_destroy(end, exit);





















	// -------------------------
	TRACE("--- thread pool lock test ---\n");
	// -------------------------
	// 

	atp_create(5, test);

	for (nIndx = 0; nIndx < 9; nIndx++) {
		atpdata = atp_alloc(data_size);
		snprintf(atpdata->s, atpdata->s_len, "lock seq (%d)", nIndx);
		if (nIndx % 2)
			atpdata->func = lock1;
		else
			atpdata->func = lock2;
		atp_addQueue(atpdata);
		TRACE("++++++++++++++++++++ thread add real Queue successed %d \n", nIndx);
	}

	sleep(7);

	TRACE("--- thread pool terminate ---\n");
	TRACE("--- thread realtime queue_size(%d), normal queue_size(%d)\n", atp_getRealtimeQueueCount(), atp_getNormalQueueCount());
#if defined(DEBUGTRACE)
	TRACE("-------------------------------------------\n");
	pThread = atp_getThreadInfo();
	for (nIndx = 0; nIndx < atp_getThreadCount(); nIndx++) {
		TRACE("threadno=%d, realtime execute=%ju average elapsed=%ju, normal execute=%ju average elapsed=%ju\n"
			, pThread[nIndx].nThreadNo
			, pThread[nIndx].nRealtimeCount
			, atp_getAverageRealtimeWorkingtime(nIndx)	// 평균 작업소요 시간
			, pThread[nIndx].nNormalCount
			, atp_getAverageNormalWorkingtime(nIndx) // 평균 작업소요 시간
		);
	}
	TRACE("-------------------------------------------\n");
#endif

	atp_destroy(end, exit);

	return 0;
}