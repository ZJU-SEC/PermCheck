# Permission Static Analysis Framework for Linux Kernel

This checker figures out critical resource(callee of direct/indirect callsite,
global variable use, interesting struct type and field use) by looking at existing
CAP/LSM/DAC check, then explore which path that uses such resource is not guarded by
those check.

This version contains the automatic detection of permission checks.

# prerequisites

* LLVM-6/7/8/9/10
* compiler with C++14 support

# build

./build.sh

# usage

```
opt \
    -analyze \
    -load=build/gatlin/libgatlin.so \
    -gatlin \
    -gating=cap \
    -ccv=0 -ccf=1 -cct=0\
    -ccvv=0 -ccfv=0 -cctv=0\
    -cvf=0 \
    -skipfun=skip.fun \
    -skipvar=skip.var \
    -lsmhook=lsm.hook \
    -prt-good=0 -prt-bad=1 -prt-ign=0 \
    -stats \
    vmlinux.bc \
    -o /dev/null 2>&1 | tee log
```

# options
* gating - gating function: cap/lsm/dac, default: cap
* ccv - check critical variables, default: 0
* ccf - check critical functions, default: 1
* cct - check critical type fields, default 0
* ccfv - print path to critical function during collect phase, default 0
* ccvv - print path to critical variable during collect phase, default 0
* cctv - print path to critical type field during collect phase, default 0
* f2c - print critical function to capability mapping, default 1
* v2c - print critical variable to capability mapping, default 1
* t2c - print critical type field to capability mapping, default 1
* caw - print check functions and wrappers discovered, default 1
* kinit - print kernel init functions, default 1
* nkinit - print kernel non init functions, default 1
* kmi - dump kernel interface, default 0
* dkmi - dump dkmi result, default 0
* cvf - complex value flow, default 0
* skipfun - list of functions don't care
* skipvar - list of variables don't care
* capfunc - list of capability check functions
* lsmhook - list of LSM hook
* critsym - list of symbols to be treated as critical and ignore others
* kapi - list of kernel api
* prt-good - print good path, default 0
* prt-bad - print bad path, default 1
* prt-ign - print ignored path, default 0
* wcapchk-kinit - warn capability check during kernel boot process, default 0
* fwd-depth - forward search max depth, default 100
* bwd-depth - backward search max depth, default 100
* svfbudget - # of iterations for cvf graph update, default 5

# vmlinux.bc

You need to install wllvm(https://github.com/travitch/whole-program-llvm)
and then use the following command to generate a single bc file.

```
~/linux: make defconfig
~/linux: make CC=wllvm
~/linux: extract-bc vmlinux
```

# Misc: where are the checks, which module should be builtin

* DAC: they are mainly used in file systems(vfs),
       stage/luster and net/sunrpc also have some checks
* LSM: those LSM hooks are scattered around in net/fs/mm/core
* CAP: capability checks are also scattered in different parts of the kernel,
       besides net/fs/mm/core, lots of device drivers also use capability checks

# I want debug info

```
CONFIG_DEBUG_INFO=y
```

# resolve indirect call: KMI or CVF

There are two ways to resolve indirect call: KMI and CVF

* KMI: kernel module interface, is built upon human knowledge of linux kernel,
the observation is that most of the callee of indirect callsite is read from
a constant struct which statically stores a function pointer, 
by matching those struct type and indicies we can match indirect call
fairly accurate(over approximate)

* CVF: this is built upon SVF, and can accurately figure out callee for indirect call,
however this is very slow and memory hungry.
CVF can process a module with ~40k functions in one hour on an Intel Xeon 6132 2.6GHz CPU.

# kernel config
--------------

## Kernel v4.18

1) ```kernel_config/allyesconfig1.config```

generated by ```make allyesconfig```

9978 yes in total

2) ```kernel_config/allyesconfig2.config```

* \- ```AMD_GPU, KASAN, UBSAN, I915, COMPILE_TEST, KEXEC_FILE, KCOV```
* \+ ```DEBUG_INFO``` 

* ```KCOV``` inserts ```__sanitizer_cov_*``` which don't have proper debug info
and will cause llvm-link fail
* ```AMD_GPU``` won't compile,
* ```I915 and KEXEC_FILE``` won't link, because of wchar issue
* ```COMPILE_TEST``` is conflict with ```DEBUG_INFO```,
* ```KASAN, UBSAN``` is irrelevant

8469 yes in total

3) ```kernel_config/allyesconfig3.config```

* \- ```AMD_GPU, I915, KEXEC, KCOV```, same reason as 2)
* \+ ```DEBUG_INFO, COMPILE_TEST```, need to patch DEBUG_INFO to not depend on ```! COMPILE_TEST```

9938 yes in total

* Clang complains 

```
inlinable function call in a function with debug info must have a !dbg location
```

you can use ```opt -strip-debug``` to remove debug info from module completely, 
so that it won't complain

## Kernel 4.18.5

1) allyesconfig1: 9975 yes

2) allyesconfig3: 9939 yes

* \+ DEBUG_INFO
* \- AMDGPU, I915, KCOV, KEXEC_FILE





# Permission checks

The results may contains false positives

# defconfig v4.18.5

## DAC

```
DAC Basic: 
selinux_inode_permission
inode_permission
xattr_permission
posix_acl_permission
may_open
generic_permission
proc_tid_comm_permission
security_file_permission
kernfs_iop_permission
proc_sys_permission
nfs_permission
selinux_file_permission
security_inode_permission



DAC Inner: 
inode_permission
posix_acl_permission
security_inode_permission
generic_permission



DAC Outer: 
inode_permission
xattr_permission
generic_permission
nfs_permission
kernfs_iop_permission
proc_fd_permission
proc_pid_permission
proc_tid_comm_permission
```



## Capability

```
CAP Basic: 
ns_capable_noaudit
netlink_net_capable
security_capable
file_ns_capable
security_capable_noaudit
cap_capable
capable
has_ns_capability_noaudit
has_ns_capability
sk_ns_capable
ns_capable
__netlink_ns_capable
netlink_ns_capable
has_capability_noaudit
netlink_capable
capable_wrt_inode_uidgid



CAP Inner: 
ns_capable
file_ns_capable
security_capable_noaudit
security_capable



CAP Outer: 
netlink_capable
netlink_ns_capable
has_capability_noaudit
sk_ns_capable
has_ns_capability
sk_net_capable
netlink_net_capable
sk_capable
ns_capable_noaudit
has_capability
ns_capable
__netlink_ns_capable
file_ns_capable
capable_wrt_inode_uidgid
has_ns_capability_noaudit
capable
```



## LSM

```
security_socket_socketpair
security_sock_rcv_skb
security_sock_graft
security_sk_clone
security_secmark_relabel_packet
security_secmark_refcount_inc
security_secmark_refcount_dec
security_secid_to_secctx
security_kernel_act_as
security_inode_listsecurity
security_kernel_module_request
security_task_setpgid
security_task_getsid
security_unix_stream_connect
security_task_setnice
security_task_setioprio
security_task_setrlimit
security_inode_init_security
security_task_movememory
security_inode_getsecctx
security_task_free
security_task_to_inode
security_cred_alloc_blank
security_ipc_permission
security_msg_msg_alloc
security_msg_queue_msgctl
security_socket_sendmsg
security_msg_queue_alloc
security_task_getpgid
security_msg_queue_msgrcv
security_msg_queue_associate
security_mmap_file
security_shm_free
security_kernel_create_files_as
security_msg_queue_free
security_socket_getsockname
security_task_alloc
security_task_prctl
security_inode_copy_up_xattr
security_cred_free
security_sctp_sk_clone
security_file_send_sigiotask
security_inode_getsecid
security_socket_recvmsg
security_inode_create
security_socket_create
security_inode_rename
security_settime64
security_netlink_send
security_task_prlimit
security_socket_getpeersec_stream
security_sctp_bind_connect
security_sb_clone_mnt_opts
security_msg_msg_free
security_socket_setsockopt
security_task_getscheduler
security_inet_csk_clone
security_sem_associate
security_file_set_fowner
security_sem_free
security_sk_free
security_socket_getsockopt
security_task_kill
security_file_open
security_socket_getpeername
security_transfer_creds
security_inode_mkdir
security_setprocattr
security_key_getsecurity
security_socket_listen
security_socket_connect
security_task_setscheduler
security_socket_accept
security_inode_invalidate_secctx
security_socket_bind
security_socket_post_create
security_file_fcntl
security_sk_alloc
security_sem_alloc
security_tun_dev_free_security
security_file_lock
security_shm_shmat
security_dentry_init_security
security_tun_dev_open
security_shm_shmctl
security_file_mprotect
security_inode_notifysecctx
security_unix_may_send
security_shm_associate
security_mmap_addr
security_inode_setattr
security_file_permission
security_inode_getsecurity
security_ipc_getsecid
security_binder_set_context_mgr
security_audit_rule_known
security_inode_removexattr
security_audit_rule_init
security_inode_listxattr
security_sb_remount
security_inode_getxattr
security_socket_getpeersec_dgram
security_sb_free
security_key_permission
security_getprocattr
security_inode_post_setxattr
security_inode_link
security_sem_semop
security_key_free
security_inode_setxattr
security_msg_queue_msgsnd
security_inode_free
security_task_getsecid
security_bprm_committed_creds
security_key_alloc
security_sem_semctl
security_inode_getattr
security_task_fix_setuid
security_inode_alloc
security_inode_permission
security_ptrace_traceme
security_inode_follow_link
security_ptrace_access_check
security_inode_readlink
security_socket_shutdown
security_inode_mknod
security_syslog
security_sb_parse_opts_str
security_inet_conn_request
security_inode_rmdir
security_quota_on
security_inode_copy_up
security_inode_symlink
security_quotactl
security_inode_unlink
security_sb_pivotroot
security_inode_setsecurity
security_sb_umount
security_req_classify_flow
security_sk_classify_flow
security_sb_statfs
security_sb_show_options
security_sb_kern_mount
security_sb_alloc
security_tun_dev_alloc_security
security_file_receive
security_bprm_committing_creds
security_capget
security_file_ioctl
security_shm_alloc
security_capable_noaudit
security_file_free
security_capable
security_file_alloc
security_capset
security_sb_mount
security_binder_transfer_file
security_audit_rule_match
security_inode_killpriv
security_binder_transfer_binder
security_audit_rule_free
security_prepare_creds
security_inode_need_killpriv
security_binder_transaction
security_tun_dev_attach
security_bprm_check
security_cred_getsecid
security_bprm_set_creds
security_d_instantiate
security_tun_dev_create
security_vm_enough_memory_mm
security_dentry_create_files_as
security_task_getioprio
security_inet_conn_established
security_inode_setsecctx
security_ismaclabel
security_kernel_post_read_file
security_kernel_read_file
security_old_inode_init_security
security_release_secctx
security_sb_copy_data
security_sb_set_mnt_opts
security_sctp_assoc_request
security_secctx_to_secid
```



wrapper

```
acpi_scan_bus_check
selinux_key_permission
cap_ptrace_traceme
tty_check_change
__check_sticky
selinux_socket_unix_may_send
may_delete
md_check_recovery
proc_fd_permission
net_ctl_permissions
nfs4_negotiate_security
nfs_permission
selinux_ptrace_access_check
cred_has_capability
file_map_prot_check
sr_block_check_events
sd_check_events
capable_wrt_inode_uidgid
selinux_inode_init_security
__ptrace_may_access
selinux_ipc_permission
inode_permission
sel_read_policycap
may_create
ptrace_may_access
sk_filter_trim_cap
selinux_dentry_init_security
secmark_tg_check
proc_pid_permission
check_nnp_nosuid
has_capability_noaudit
file_ns_capable
check_disk_size_change
ptracer_capable
has_ns_capability_noaudit
has_capability
ns_capable
ipcperms
capable
avc_has_extended_perms
kernfs_iop_permission
ns_capable_noaudit
proc_tid_comm_permission
disk_check_events
selinux_inode_permission
may_open
avc_has_perm_flags
avc_has_perm
netlink_capable
ext4_init_security
cap_ptrace_access_check
cap_convert_nscap
cap_inode_removexattr
drm_dp_check_mstb_guid
__netlink_ns_capable
netlink_net_capable
sk_capable
sk_net_capable
sk_ns_capable
generic_permission
__nv_msi_ht_cap_quirk
__key_link_check_live_key
avc_has_perm_noaudit
check_kill_permission
nv_msi_ht_cap_quirk_all
cookie_v4_check
cgroup_procs_write_permission
has_ns_capability
nfs_clone_sb_security
quirk_nvidia_ck804_msi_ht_cap
check_ctrlrecip
i915_hangcheck_elapsed
hpet_msi_capability_lookup
cookie_v6_check
usb3_lpm_permit_store
intel_dp_check_mst_status
check_disk_change
nv_msi_ht_cap_quirk_leaf
xattr_permission
drm_dp_check_and_send_link_address
tcp_check_req
netlink_ns_capable
__tty_check_change
key_task_permission
selinux_file_permission
selinux_capable
security_get_classes
security_context_str_to_sid
security_context_to_sid_force
security_sid_to_context
security_port_sid
security_add_hooks
security_load_policy
security_compute_av
security_compute_av_user
security_get_reject_unknown
security_get_user_sids
security_sid_to_context_core
security_compute_validatetrans
security_get_initial_sid_context
security_read_policy
security_fs_use
security_compute_sid
security_policydb_len
security_ib_endport_sid
security_node_sid
security_get_bools
security_set_bools
security_sid_to_context_force
security_transition_sid
security_sid_mls_copy
security_get_bool_value
security_context_to_sid
security_netlbl_secattr_to_sid
security_ib_pkey_sid
security_get_permissions
security_context_to_sid_default
security_validate_transition
security_change_sid
security_is_socket_class
security_net_peersid_resolve
security_policycap_supported
security_bounded_transition
security_genfs_sid
security_netif_sid
security_load_policycaps
security_tun_dev_attach_queue
security_netlbl_sid_to_secattr
security_transition_sid_user
security_validate_transition_user
security_get_allow_unknown
security_init
security_context_to_sid_core
security_member_sid
security_compute_xperms_decision
security_module_enable
security_mls_enabled
```





# allyesconfig v4.18.5

## dac

```
DAC Basic:
aa_sock_file_perm
btrfs_permission
security_inode_permission
aa_profile_af_perm
ocfs2_permission
fuse_permission
cifs_permission
security_file_permission
aa_may_ptrace
nfsd_permission
coda_ioctl_permission
ceph_permission
ovl_permission
kernfs_iop_permission
proc_tid_comm_permission
coda_permission
gfs2_permission
nilfs_permission
orangefs_permission
inode_permission
aa_label_sk_perm
aa_path_perm
afs_permission
xattr_permission
generic_permission
aa_file_perm
posix_acl_permission
proc_sys_permission
selinux_inode_permission
aa_sk_perm
selinux_file_permission
nfs_permission
smack_inode_permission
may_open
aa_check_perms
aa_may_manage_policy
aa_sock_msg_perm


DAC Inner:
a_sock_file_perm
aa_profile_af_perm
aa_sk_perm
generic_permission
aa_check_perms
security_inode_permission
posix_acl_permission
inode_permission
aa_label_sk_perm
aa_may_manage_policy


DAC Outer:
kobj_ns_current_may_mount
pcie_capability_read_dword
aa_may_signal
mlx5_query_port_proto_cap
aa_may_manage_policy
rxe_qp_chk_cap
__aa_path_perm
aa_sock_perm
proc_fd_permission
ns_capable
vivid_vid_cap_overlay
security_net_peersid_resolve
kernfs_iop_permission
netlink_ns_capable
__devcgroup_check_permission
tomoyo_socket_connect_permission
aa_profile_label_perm
ecryptfs_permission
fuse_permission
apparmor_capable
aa_str_perms
security_kernel_post_read_file
security_context_to_sid_force
ns_capable_noaudit
sk_net_capable
sk_capable
sk_ns_capable
netlink_net_capable
netlink_capable
aa_sock_file_perm
aa_profile_af_perm
aa_sk_perm
generic_permission
aa_check_perms
security_inode_permission
posix_acl_permission
inode_permission
aa_label_sk_perm
aa_may_manage_policy
aa_sock_file_perm
btrfs_permission
security_inode_permission
aa_profile_af_perm
ocfs2_permission
fuse_permission
cifs_permission
security_file_permission
aa_may_ptrace
nfsd_permission
coda_ioctl_permission
ceph_permission
ovl_permission
kernfs_iop_permission
proc_tid_comm_permission
coda_permission
gfs2_permission
nilfs_permission
orangefs_permission
inode_permission
aa_label_sk_perm
aa_path_perm
afs_permission
xattr_permission
generic_permission
aa_file_perm
posix_acl_permission
proc_sys_permission
selinux_inode_permission
aa_sk_perm
selinux_file_permission
nfs_permission
smack_inode_permission
may_open
aa_check_perms
aa_may_manage_policy
aa_sock_msg_perm
selinux_file_permission
avc_has_perm_flags
aa_label_sk_perm
avc_has_perm
aa_file_perm
proc_pid_permission
security_context_to_sid
igb_read_pcie_cap_reg
fimc_cap_s_input
security_kernel_read_file
proc_cap_handler
selinux_capable
pcie_capability_read_word
xattr_permission
security_sid_to_context_core
aa_sock_opt_perm
aa_capable
cred_has_capability
file_has_perm
file_ns_capable
gfs2_permission
ocfs2_permission
btrfs_permission
apparmor_file_permission
__netlink_ns_capable
aa_sk_perm
security_node_sid
capable_wrt_inode_uidgid
aa_path_perm
aa_sock_file_perm
proc_tid_comm_permission
__send_cap
avc_has_extended_perms
orangefs_permission
check_kill_permission
selinux_key_permission
__nv_msi_ht_cap_quirk
security_sid_to_context
capable
reiserfs_permission
security_sid_to_context_force
pci_find_next_ext_capability
security_context_to_sid_core
ceph_cap_op_name
ceph_permission
security_context_to_sid_default
wlc_lcnphy_samp_cap
inode_permission
avc_has_perm_noaudit
generic_permission
aa_may_ptrace
aa_af_perm
```

## CAP

```
CAP Basic: 
ns_capable_noaudit
netlink_ns_capable
netlink_net_capable
sk_ns_capable
__netlink_ns_capable
has_capability
capable
smack_privileged
aa_capable
security_capable
file_ns_capable
cap_capable
has_capability_noaudit
smack_privileged_cred
security_capable_noaudit
netlink_capable
has_ns_capability_noaudit
has_ns_capability
ns_capable
map_write
capable_wrt_inode_uidgid


CAP Inner: 
has_ns_capability
cap_capable
smack_privileged_cred
file_ns_capable
ns_capable
has_ns_capability_noaudit
security_capable_noaudit
security_capable



CAP Outer:
netlink_capable
smack_privileged
clear_feat_caps
avc_has_perm_noaudit
cpcap_rtc_alarm_irq_enable
wlc_lcnphy_samp_cap
avc_has_perm_flags
avc_has_perm
snd_hda_check_amp_caps
tcpm_validate_caps
security_context_to_sid
igb_read_pcie_cap_reg
fimc_cap_s_input
security_kernel_read_file
selinux_capable
pcie_capability_read_word
r8712_secmicappend
security_sid_to_context_core
powercap_register_zone
drbg_kcapi_seed
aa_capable
file_has_perm
__send_cap
__ceph_caps_issued_mask
file_ns_capable
pcie_capability_read_dword
__netlink_ns_capable
test_string_unescape
security_node_sid
renewed_caps
avc_has_extended_perms
capable_wrt_inode_uidgid
cred_has_capability
apparmor_capable
check_kill_permission
selinux_key_permission
drbg_kcapi_sym_ctr
snd_hda_override_amp_caps
capincci_free
security_sid_to_context
query_amp_caps
capable
security_sid_to_context_force
aa_may_signal
sk_capable
sk_net_capable
pci_find_next_ext_capability
security_context_to_sid_core
ceph_cap_op_name
aa_af_perm
test_string_escape
__nv_msi_ht_cap_quirk
lwtunnel_valid_encap_type_attr
proc_cap_handler
kobj_ns_current_may_mount
selinux_file_permission
netlink_net_capable
sk_ns_capable
drbg_kcapi_random
security_context_to_sid_default
capi_unlocked_ioctl
cpcap_map_mode
sii8620_got_xdevcap
isdn_concap_new
has_capability_noaudit
mlx5_query_port_proto_cap
rxe_qp_chk_cap
aa_profile_af_perm
nand_check_ecc_caps
has_ns_capability
pcap_rtc_alarm_irq_enable
capilib_new_ncci
ns_capable
has_ns_capability_noaudit
has_capability
vivid_vid_cap_overlay
smack_privileged_cred
security_net_peersid_resolve
ip_tunnel_encap_del_ops
netlink_ns_capable
ice_parse_caps
__devcgroup_check_permission
ip6_tnl_encap_del_ops
tomoyo_socket_connect_permission
decap_and_validate
aa_str_perms
security_kernel_post_read_file
security_context_to_sid_force
ns_capable_noaudit
encapsulate
tomoyo_socket_bind_permission
```





## LSM

```
security_socket_socketpair
security_sock_rcv_skb
security_sock_graft
security_sk_clone
security_secmark_relabel_packet
security_secmark_refcount_inc
security_secmark_refcount_dec
security_secid_to_secctx
security_kernel_act_as
security_inode_listsecurity
security_kernel_module_request
security_task_setpgid
security_task_getsid
security_unix_stream_connect
security_task_setnice
security_task_setioprio
security_task_setrlimit
security_inode_init_security
security_task_movememory
security_inode_getsecctx
security_task_free
security_task_to_inode
security_cred_alloc_blank
security_ipc_permission
security_msg_msg_alloc
security_msg_queue_msgctl
security_socket_sendmsg
security_msg_queue_alloc
security_task_getpgid
security_msg_queue_msgrcv
security_msg_queue_associate
security_mmap_file
security_shm_free
security_kernel_create_files_as
security_msg_queue_free
security_socket_getsockname
security_task_alloc
security_task_prctl
security_inode_copy_up_xattr
security_cred_free
security_sctp_sk_clone
security_file_send_sigiotask
security_inode_getsecid
security_socket_recvmsg
security_inode_create
security_socket_create
security_inode_rename
security_settime64
security_netlink_send
security_task_prlimit
security_socket_getpeersec_stream
security_sctp_bind_connect
security_sb_clone_mnt_opts
security_msg_msg_free
security_socket_setsockopt
security_task_getscheduler
security_inet_csk_clone
security_sem_associate
security_file_set_fowner
security_sem_free
security_sk_free
security_socket_getsockopt
security_task_kill
security_file_open
security_socket_getpeername
security_transfer_creds
security_inode_mkdir
security_setprocattr
security_key_getsecurity
security_socket_listen
security_socket_connect
security_task_setscheduler
security_socket_accept
security_inode_invalidate_secctx
security_socket_bind
security_socket_post_create
security_file_fcntl
security_sk_alloc
security_sem_alloc
security_tun_dev_free_security
security_file_lock
security_shm_shmat
security_dentry_init_security
security_tun_dev_open
security_shm_shmctl
security_file_mprotect
security_inode_notifysecctx
security_unix_may_send
security_shm_associate
security_mmap_addr
security_inode_setattr
security_file_permission
security_inode_getsecurity
security_ipc_getsecid
security_binder_set_context_mgr
security_audit_rule_known
security_inode_removexattr
security_audit_rule_init
security_inode_listxattr
security_sb_remount
security_inode_getxattr
security_socket_getpeersec_dgram
security_sb_free
security_key_permission
security_getprocattr
security_inode_post_setxattr
security_inode_link
security_sem_semop
security_key_free
security_inode_setxattr
security_msg_queue_msgsnd
security_inode_free
security_task_getsecid
security_bprm_committed_creds
security_key_alloc
security_sem_semctl
security_inode_getattr
security_task_fix_setuid
security_inode_alloc
security_inode_permission
security_ptrace_traceme
security_inode_follow_link
security_ptrace_access_check
security_inode_readlink
security_socket_shutdown
security_inode_mknod
security_syslog
security_sb_parse_opts_str
security_inet_conn_request
security_inode_rmdir
security_quota_on
security_inode_copy_up
security_inode_symlink
security_quotactl
security_inode_unlink
security_sb_pivotroot
security_inode_setsecurity
security_sb_umount
security_req_classify_flow
security_sk_classify_flow
security_sb_statfs
security_sb_show_options
security_sb_kern_mount
security_sb_alloc
security_tun_dev_alloc_security
security_file_receive
security_bprm_committing_creds
security_capget
security_file_ioctl
security_shm_alloc
security_capable_noaudit
security_file_free
security_capable
security_file_alloc
security_capset
security_sb_mount
security_binder_transfer_file
security_audit_rule_match
security_inode_killpriv
security_binder_transfer_binder
security_audit_rule_free
security_prepare_creds
security_inode_need_killpriv
security_binder_transaction
security_tun_dev_attach
security_bprm_check
security_cred_getsecid
security_bprm_set_creds
security_d_instantiate
security_tun_dev_create
security_vm_enough_memory_mm
security_dentry_create_files_as
security_task_getioprio
security_inet_conn_established
security_inode_setsecctx
security_ismaclabel
security_kernel_post_read_file
security_kernel_read_file
security_old_inode_init_security
security_release_secctx
security_sb_copy_data
security_sb_set_mnt_opts
security_sctp_assoc_request
security_secctx_to_secid
security_bpf
security_bpf_map
security_bpf_prog
security_bpf_map_alloc
security_bpf_prog_alloc
security_bpf_map_free
security_bpf_prog_free
security_xfrm_policy_alloc
security_xfrm_policy_clone
security_xfrm_policy_free
security_xfrm_policy_delete
security_xfrm_state_alloc
security_xfrm_state_alloc_acquire
security_xfrm_state_delete
security_xfrm_state_free
security_xfrm_policy_lookup
security_xfrm_state_pol_flow_match
security_xfrm_decode_session
security_skb_classify_flow
security_ib_pkey_access
security_ib_endport_manage_subnet
security_ib_alloc_security
security_ib_free_security
security_path_mknod
security_path_mkdir
security_path_rmdir
security_path_unlink
security_path_symlink
security_path_link
security_path_rename
security_path_truncate
security_path_chmod
security_path_chown
security_path_chroot
```



wrapper 

```
may_destroy_subvol
ocfs2_calc_security_init
tomoyo_env_perm
tomoyo_execute_permission
nfs4_negotiate_security
vidioc_s_fmt_vid_cap.1807765
tomoyo_bprm_check_security
cifs_permission
sigio_perm
cap11xx_i2c_probe
avc_has_perm
securityfs_create_dentry
cap_ptrace_access_check
__ptrace_may_access
oprofilefs_create_file_perm
reiserfs_security_write
selinux_key_permission
selinux_ipc_permission
selinux_capable
drbd_may_finish_epoch
ib_security_modify_qp
ptracer_capable
gtp_encap_destroy
has_capability
has_capability_noaudit
qib_cap_mask_chg
cap_inode_removexattr
netlink_capable
file_has_perm
sk_capable
ib_destroy_qp_security_abort
hfsplus_init_security
hfsplus_init_inode_security
file_ns_capable
ecryptfs_permission
xattr_permission
may_open
ib_close_shared_qp_security
ib_mad_agent_security_cleanup
cachefiles_determine_cache_security
reiserfs_security_init
nfsd_permission
securityfs_create_symlink
cap_ptrace_traceme
selinux_dentry_init_security
ptrace_may_access
btrfs_permission
check_kill_permission
ib_mad_agent_security_change
cap_safe_nice
ib_create_qp_security
gb_cap_connection_exit
ipcperms
avc_has_extended_perms
ns_capable_noaudit
xfs_init_security
proc_fd_permission
key_task_permission
has_ns_capability_noaudit
nfs_permission
generic_permission
hpet_msi_capability_lookup
avc_has_perm_noaudit
ib_mad_enforce_security
ext2_init_security
selinux_inode_init_security
btrfs_xattr_security_init
may_create
may_delete
selinux_file_permission
selinux_inode_permission
netlink_ns_capable
gfs2_permission
ocfs2_init_security_and_acl
has_ns_capability
ubifs_init_security
reiserfs_permission
orangefs_permission
capable
nfs_may_open
ib_open_shared_qp_security
afs_permission
f2fs_init_security
ocfs2_permission
kernfs_iop_permission
ib_destroy_qp_security_end
inode_permission
tomoyo_mount_permission
ovl_permission
securityfs_create_dir
proc_pid_permission
ns_capable
sk_net_capable
selinux_socket_unix_may_send
fuse_permission
avc_has_perm_flags
ib_security_cache_change
cap_convert_nscap
capable_wrt_inode_uidgid
ext4_init_security
rxrpc_init_server_conn_security
sk_ns_capable
hfi1_cap_mask_chg
vxlan_encap_bypass
sk_filter_trim_cap
ceph_permission
powercap_register_zone
nfs4_server_capabilities
powercap_unregister_control_type
cred_has_capability
cgroup_procs_write_permission
gtp_encap_disable
ip6table_security_table_init
__netlink_ns_capable
jffs2_init_security
nilfs_permission
rapl_package_register_powercap
netlink_net_capable
nfs_clone_sb_security
proc_tid_comm_permission
securityfs_create_file
vimc_cap_comp_unbind
create_sscape
net_ctl_permissions
fscrypt_has_permitted_context
iptable_security_table_init
btrfs_may_alloc_data_chunk
jfs_init_security
afs_check_permit
cpia2_s_fmt_vid_cap
security_ib_pkey_sid
security_transition_sid
security_read_policy
security_ib_endport_sid
security_policydb_len
security_init
security_context_to_sid_force
security_bounded_transition
security_sid_to_context_core
security_mls_enabled
security_sid_mls_copy
security_get_bool_value
security_netlbl_secattr_to_sid
security_load_policy
security_get_user_sids
security_node_sid
security_sid_to_context
security_set_bools
security_get
security_get_allow_unknown
security_list
security_get_permissions
security_member_sid
security_context_str_to_sid
security_genfs_sid
security_context_to_sid
security_module_enable
security_net_peersid_resolve
security_context_to_sid_core
security_compute_validatetrans
security_compute_xperms_decision
security_change_sid
security_show
security_context_to_sid_default
security_set
security_get_initial_sid_context
security_compute_av
security_compute_sid
security_is_socket_class
security_fs_use
security_sid_to_context_force
security_netif_sid
security_policycap_supported
security_get_bools
security_netlbl_sid_to_secattr
security_port_sid
security_validate_transition
security_tun_dev_attach_queue
security_transition_sid_user
security_get_classes
security_load_policycaps
security_add_hooks
security_validate_transition_user
security_get_reject_unknown
security_compute_av_user
```

