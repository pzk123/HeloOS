/* ---------------------------------

	HELO OS 系统专用源程序

----------------------------------- */

#include "bootpack.h"

struct TASKCTL *taskctl;
struct TIMER *task_timer;

struct TASK *task_now(void)
{
	struct TASKLEVEL *tl = &taskctl->level[taskctl->now_lv];
	return tl->tasks[tl->now];
}

void task_add(struct TASK *task)
{
	struct TASKLEVEL *tl = &taskctl->level[task->level];
	tl->tasks[tl->running] = task;
	tl->running++;
	task->flags = 2; /* 摦嶌拞 */
	return;
}

void task_remove(struct TASK *task)
{
	int i;
	struct TASKLEVEL *tl = &taskctl->level[task->level];

	/* task偑偳偙偵偄傞偐傪扵偡 */
	for (i = 0; i < tl->running; i++) {
		if (tl->tasks[i] == task) {
			/* 偙偙偵偄偨 */
			break;
		}
	}

	tl->running--;
	if (i < tl->now) {
		tl->now--; /* 偢傟傞偺偱丄偙傟傕偁傢偣偰偍偔 */
	}
	if (tl->now >= tl->running) {
		/* now偑偍偐偟側抣偵側偭偰偄偨傜丄廋惓偡傞 */
		tl->now = 0;
	}
	task->flags = 1; /* 僗儕乕僾拞 */

	/* 偢傜偟 */
	for (; i < tl->running; i++) {
		tl->tasks[i] = tl->tasks[i + 1];
	}

	return;
}

void task_switchsub(void)
{
	int i;
	/* 堦斣忋偺儗儀儖傪扵偡 */
	for (i = 0; i < MAX_TASKLEVELS; i++) {
		if (taskctl->level[i].running > 0) {
			break; /* 尒偮偐偭偨 */
		}
	}
	taskctl->now_lv = i;
	taskctl->lv_change = 0;
	return;
}

void task_idle(void)
{
	for (;;) {
		io_hlt();
	}
}

struct TASK *task_init(struct MEMMAN *memman)
{
	int i;
	struct TASK *task, *idle;
	struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *) ADR_GDT;

	taskctl = (struct TASKCTL *) memman_alloc_4k(memman, sizeof (struct TASKCTL));
	for (i = 0; i < MAX_TASKS; i++) {
		taskctl->tasks0[i].flags = 0;
		taskctl->tasks0[i].sel = (TASK_GDT0 + i) * 8;
		taskctl->tasks0[i].tss.ldtr = (TASK_GDT0 + MAX_TASKS + i) * 8;
		set_segmdesc(gdt + TASK_GDT0 + i, 103, (int) &taskctl->tasks0[i].tss, AR_TSS32);
		set_segmdesc(gdt + TASK_GDT0 + MAX_TASKS + i, 15, (int) taskctl->tasks0[i].ldt, AR_LDT);
	}
	for (i = 0; i < MAX_TASKLEVELS; i++) {
		taskctl->level[i].running = 0;
		taskctl->level[i].now = 0;
	}

	task = task_alloc();
	task->flags = 2;	/* 摦嶌拞儅乕僋 */
	task->priority = 2; /* 0.02昩 */
	task->level = 0;	/* 嵟崅儗儀儖 */
	task_add(task);
	task_switchsub();	/* 儗儀儖愝掕 */
	load_tr(task->sel);
	task_timer = timer_alloc();
	timer_settime(task_timer, task->priority);

	idle = task_alloc();
	idle->tss.esp = memman_alloc_4k(memman, 64 * 1024) + 64 * 1024;
	idle->tss.eip = (int) &task_idle;
	idle->tss.es = 1 * 8;
	idle->tss.cs = 2 * 8;
	idle->tss.ss = 1 * 8;
	idle->tss.ds = 1 * 8;
	idle->tss.fs = 1 * 8;
	idle->tss.gs = 1 * 8;
	task_run(idle, MAX_TASKLEVELS - 1, 1);

	return task;
}

struct TASK *task_alloc(void)
{
	int i;
	struct TASK *task;
	for (i = 0; i < MAX_TASKS; i++) {
		if (taskctl->tasks0[i].flags == 0) {
			task = &taskctl->tasks0[i];
			task->flags = 1; /* 巊梡拞儅乕僋 */
			task->tss.eflags = 0x00000202; /* IF = 1; */
			task->tss.eax = 0; /* 偲傝偁偊偢0偵偟偰偍偔偙偲偵偡傞 */
			task->tss.ecx = 0;
			task->tss.edx = 0;
			task->tss.ebx = 0;
			task->tss.ebp = 0;
			task->tss.esi = 0;
			task->tss.edi = 0;
			task->tss.es = 0;
			task->tss.ds = 0;
			task->tss.fs = 0;
			task->tss.gs = 0;
			task->tss.iomap = 0x40000000;
			task->tss.ss0 = 0;
			return task;
		}
	}
	return 0; /* 傕偆慡晹巊梡拞 */
}

void task_run(struct TASK *task, int level, int priority)
{
	if (level < 0) {
		level = task->level; /* 儗儀儖傪曄峏偟側偄 */
	}
	if (priority > 0) {
		task->priority = priority;
	}

	if (task->flags == 2 && task->level != level) { /* 摦嶌拞偺儗儀儖偺曄峏 */
		task_remove(task); /* 偙傟傪幚峴偡傞偲flags偼1偵側傞偺偱壓偺if傕幚峴偝傟傞 */
	}
	if (task->flags != 2) {
		/* 僗儕乕僾偐傜婲偙偝傟傞応崌 */
		task->level = level;
		task_add(task);
	}

	taskctl->lv_change = 1; /* 師夞僞僗僋僗僀僢僠偺偲偒偵儗儀儖傪尒捈偡 */
	return;
}

void task_sleep(struct TASK *task)
{
	struct TASK *now_task;
	if (task->flags == 2) {
		/* 摦嶌拞偩偭偨傜 */
		now_task = task_now();
		task_remove(task); /* 偙傟傪幚峴偡傞偲flags偼1偵側傞 */
		if (task == now_task) {
			/* 帺暘帺恎偺僗儕乕僾偩偭偨偺偱丄僞僗僋僗僀僢僠偑昁梫 */
			task_switchsub();
			now_task = task_now(); /* 愝掕屻偱偺丄乽尰嵼偺僞僗僋乿傪嫵偊偰傕傜偆 */
			farjmp(0, now_task->sel);
		}
	}
	return;
}

void task_switch(void)
{
	struct TASKLEVEL *tl = &taskctl->level[taskctl->now_lv];
	struct TASK *new_task, *now_task = tl->tasks[tl->now];
	tl->now++;
	if (tl->now == tl->running) {
		tl->now = 0;
	}
	if (taskctl->lv_change != 0) {
		task_switchsub();
		tl = &taskctl->level[taskctl->now_lv];
	}
	new_task = tl->tasks[tl->now];
	timer_settime(task_timer, new_task->priority);
	if (new_task != now_task) {
		farjmp(0, new_task->sel);
	}
	return;
}
