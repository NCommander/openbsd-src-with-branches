#
# $KTH: .gdbinit,v 1.5 2000/11/02 12:41:46 lha Exp $
#


define listfcachenodes
set $foo = lrulist->head
while $foo != 0
print *(FCacheEntry *) $foo->data
set $foo = $foo->next
end
print 4711
end

document listfcachenodes
List all fcache nodes in the lru-list
end


define listfcachenodes_fid
set $foo = lrulist->head
while $foo != 0
print ((FCacheEntry *) $foo->data)->fid
set $foo = $foo->next
end
print 4711
end

document listfcachenodes_fid
List all fcache nodes's fids in the lru-list
end

define fcache_lru_num_nodes
set $bar = 0
set $foo = lrulist->head
while $foo != 0
set $bar = $bar + 1
set $foo = $foo->next
end
print $bar
end

document fcache_lru_num_nodes
Count number of nodes in the fcache lrulist
end

define fcache_lru_num_used_nodes
set $bar = 0
set $foo = lrulist->head
while $foo != 0
if ((FCacheEntry *) $foo->data)->flags.usedp != 0
set $bar = $bar + 1
end
set $foo = $foo->next
end
print $bar
end

document fcache_lru_num_used_nodes
Count number of USED nodes in the fcache lrulist
end

define lwp_ps_internal
set $lwp_ps_queue = $arg0
set $lwp_ps_counter = $lwp_ps_queue.count
set $lwp_ps_foo = $lwp_ps_queue->head
while $lwp_ps_counter != 0
  printf " name: %s", (char *) $lwp_ps_foo->name
  if $lwp_ps_foo == lwp_cpptr
    printf "                 RUNNING THREAD"
  end
  printf "\n"
  printf "  eventlist:"
  set $lwp_ps_envcnt = 0
  while $lwp_ps_envcnt < $lwp_ps_foo->eventcnt
    printf " %x",  $lwp_ps_foo->eventlist[$lwp_ps_envcnt]
    set $lwp_ps_envcnt = $lwp_ps_envcnt + 1
  end
  printf "\n"
  if $lwp_ps_foo == lwp_cpptr
    printf "  fp: 0x%x\n", $fp
    printf "  pc: 0x%x\n", $pc
    printf "  pointers on topstack added for completeness\n"
  end
  printf "  fp: 0x%x\n",  ((int *)$lwp_ps_foo->context->topstack)[2]
  printf "  pc: 0x%x\n",  ((int *)$lwp_ps_foo->context->topstack)[3]
  set $lwp_ps_foo = $lwp_ps_foo->next
  set $lwp_ps_counter = $lwp_ps_counter - 1
end
end

define lwp_ps
echo Runnable[0]\n
lwp_ps_internal runnable[0]
echo Runnable[1]\n
lwp_ps_internal runnable[1]
echo Runnable[2]\n
lwp_ps_internal runnable[2]
echo Runnable[3]\n
lwp_ps_internal runnable[3]
echo Runnable[4]\n
lwp_ps_internal runnable[4]
echo Blocked\n
lwp_ps_internal blocked
end

document lwp_ps
Print all processes, running or blocked
end

define list_count
   set $count = 0
   set $current = ((List *)$arg0)->head
   while $current != 0
     set $count = $count + 1
     set $current = $current->next
   end
   printf "List contains %d entries\n", $count
end

document list_count
Count number of elements on util LIST.
end

define volume_print
   set $current = 'volcache.c'::lrulist->head
   while $current != 0
     set $entry = (VolCacheEntry *)$current->data
     if $entry->refcount != 0
        printf "%p - %s ref: %d\n", $entry, $entry->entry->name, $entry->refcount
     end
     set $current = $current->next
   end
end

document volume_print
Print the volume in the volcache
end

define volume_count
   set $cnt = 0
   set $vol_refs = 0
   set $total_cnt = 0
   set $current = 'volcache.c'::lrulist->head
   while $current != 0
     set $entry = (VolCacheEntry *)$current->data
     if $entry->refcount != 0
	set $cnt = $cnt + 1
        set $total_cnt = $total_cnt + $entry->refcount
     end
     set $vol_refs = $vol_refs + $entry->vol_refs
     set $current = $current->next
   end
   printf "Used volcache: counted: %d count: %d max: %d\n", $cnt, nactive_volcacheentries, nvolcacheentries
   printf "Refcount total: %d, used fcache nodes are: %d\n", $total_cnt, 'fcache.c'::usedvnodes
   printf "Volrefs total to %d\n", $vol_refs
end

document volume_count
Print the number of active entries in volcache, by counting 
them and printing the accounting variables
end

define volume_check
   set $current = 'volcache.c'::lrulist->head
   while $current != 0
     set $entry = (VolCacheEntry *)$current->data
     printf "checking %s\n", $entry->entry->name

     if $entry->refcount != 0
        set $cnt = 0
        printf "  checking fcache\n"
        set $fcur = 'fcache.c'::lrulist->head
        while $fcur != 0
           if ((FCacheEntry *)$fcur->data)->volume == $entry
              set $cnt = $cnt + 1
           end
           set $fcur = $fcur->next
        end
        if $cnt != $entry->refcount
           printf " failed %d fcache entries used, while entry was accounted for %d\n", $cnt, $entry->refcount
        else	   
           printf " ok\n"	   
        end	   
     end

     set $current = $current->next
   end
end

document volume_check
Check volcache consistency WRT fcache usage, too slow to use !
end
