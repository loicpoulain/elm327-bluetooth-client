/* 
 * elm client over bluetooth/rfcomm
 *
 * Copyright (C) 2016 Loic Poulain
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

#define MAX_EP_EVTS 8

int main(int argc, char *argv[])
{
	struct epoll_event ev, events[MAX_EP_EVTS];
	struct sockaddr_rc addr;
	int cli_s, epoll_fd, err, nfds;
	int addr_len = sizeof(addr);
	int fd_in = fileno(stdin);
	int fd_out = fileno(stdout);

	if (argc < 2) {
		fprintf(stderr, "Usage:\n\telm-bt <bdaddr> [channel]\n");
		return 0;
	}

	cli_s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
	if (cli_s < 0) {
		perror("error opening BT/RFCOMM socket");
		return cli_s;
	}

	addr.rc_family = AF_BLUETOOTH;
	str2ba(argv[1], &addr.rc_bdaddr );
	addr.rc_channel = (argc < 3) ? 1 : htobs(atoi(argv[2]));

	printf("connecting to %s (channel %d)\n", argv[1], addr.rc_channel);

	err = connect(cli_s, (struct sockaddr *)&addr, addr_len);
	if (err) {
		perror("unable to connect");
		close(cli_s);
		return err;
	}

	printf("connected\n");

	epoll_fd = epoll_create(1);
	if (epoll_fd < 0) {
		perror("Unable to create epoll");
		close(cli_s);
		return epoll_fd;
	}

	ev.events = EPOLLIN;
	ev.data.fd = cli_s;
	err = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cli_s, &ev);
	if (err) {
		perror("unable to add client socket to epoll instance");
		close(cli_s);
		return err;
	}

	ev.events = EPOLLIN;
	ev.data.fd = fd_in;
	err = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd_in, &ev);
	if (err) {
		perror("unable to add fd_in to epoll instance");
		close(cli_s);
		return err;
	}

	while(1) {
		char buf[1024], *p;
		ssize_t len;
		int i;

		nfds = epoll_wait(epoll_fd, events, MAX_EP_EVTS, -1);
		if (nfds < 0) {
			perror("epoll error");
			break;
		}

		for (i = 0; i < nfds; i++) {
			if ((events[i].events & EPOLLERR) ||
			    (events[i].events & EPOLLHUP)) {
				fprintf(stderr, "epoll error\n");
				goto end;
			}

			if (events[i].data.fd == cli_s) {
				len = read(cli_s, &buf, sizeof(buf) - 1);
				if (len < 0) {
					perror("socket read error");
					continue;
				}
				buf[len] = '\0';

				p = buf;
				while(*p) {
					if (*p == '\r')
						*p = '\n';
					p++;
				}

				write(fd_out, buf, strlen(buf));
			} else if (events[i].data.fd == fd_in) {
				len = read(fd_in, &buf, sizeof(buf) - 1);
				if (len < 0) {
					perror("fd_in read error");
					continue;
				}
				buf[len] = '\0';
		
				/* All  messages  to  the ELM327  must  be
				 * terminated  with  a  carriage  return
				 * character  (hex  ‘0D’, \r).
				 */
				p = buf;
				while (*p) {
					if (*p == '\n')
						*p = '\r';
					p++;
				}

				write(cli_s, buf, len);
			} else {
				fprintf(stderr, "unknown event");
			}
		}
	}

end:
	close(cli_s);
	return 0;
}
