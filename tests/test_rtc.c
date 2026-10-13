/* libSceRtc: RFC 3339 / RFC 2822 / asctime parsing and RFC 3339 formatting. */
#undef NDEBUG /* the checks are asserts; release builds define NDEBUG */
#include "../src/runtime_rtc.c"
#include <assert.h>

uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *nid) { (void)table; (void)count; (void)nid; return 0; }

static uint64_t tick_of(int y, int mo, int d, int h, int mi, int s, uint32_t us) {
    DateTime t={(uint16_t)y,(uint16_t)mo,(uint16_t)d,(uint16_t)h,(uint16_t)mi,(uint16_t)s,us};
    return to_tick(&t);
}

int main(void) {
    uint64_t tick=0, utc=tick_of(2026,10,13,6,30,0,0);
    assert(rtc_parse_rfc3339(&tick,"2026-10-13T06:30:00Z")==0 && tick==utc);
    assert(rtc_parse_rfc3339(&tick,"2026-10-13T12:00:00+05:30")==0 && tick==utc);
    assert(rtc_parse_rfc3339(&tick,"2026-10-13T01:30:00.25-05:00")==0 && tick==utc+250000);
    assert(rtc_parse_rfc3339(&tick,"2026-13-01T00:00:00Z")==ERR_INVALID_MONTH);
    assert(rtc_parse_rfc3339(&tick,"garbage")==ERR_INVALID_VALUE);
    assert(rtc_parse_rfc3339(NULL,"2026-10-13T06:30:00Z")==ERR_INVALID_POINTER);
    assert(rtc_parse_date_time(&tick,"2026-10-13T06:30:00Z")==0 && tick==utc);
    assert(rtc_parse_date_time(&tick,"Tue, 13 Oct 2026 12:00:00 +0530")==0 && tick==utc);
    assert(rtc_parse_date_time(&tick,"Tue, 13 Oct 2026 06:30:00 GMT")==0 && tick==utc);
    assert(rtc_parse_date_time(&tick,"Tue Oct 13 06:30:00 2026")==0 && tick==utc);

    char text[40];
    assert(rtc_format_rfc3339(text,&utc,0)==0 && !strcmp(text,"2026-10-13T06:30:00.00Z"));
    assert(rtc_format_rfc3339(text,&utc,330)==0 && !strcmp(text,"2026-10-13T12:00:00.00+05:30"));
    uint64_t later=utc+250000;
    assert(rtc_format_rfc3339(text,&later,-300)==0 && !strcmp(text,"2026-10-13T01:30:00.25-05:00"));
    assert(rtc_parse_rfc3339(&tick,text)==0 && tick==later);
    puts("rtc test: ok");
    return 0;
}
