@startuml
start
:用户请求分配内存;
:ThreadCache::Allocate(size);
if (ThreadCache 有空闲块?) then (是)
:直接分配;
stop
else (否)
:向 CentralCache 申请批量内存;
:CentralCache::FetchFromCentral(size);
if (CentralCache 有空闲页?) then (是)
:拆分页，返回内存块;
:ThreadCache 分配;
stop
else (否)
:向 PageCache 申请页;
:PageCache::AllocatePage(num);
if (PageCache 有空闲页?) then (是)
:分配页;
else (否)
:向操作系统申请大块内存;
:分配页;
endif
:CentralCache 拆分页，返回内存块;
:ThreadCache 分配;
stop
endif
endif
@enduml
