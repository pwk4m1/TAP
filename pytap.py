#!/usr/local/bin/python3
import argparse
from socket import *
import select
import ssl
import struct
import string
import sys
import threading
from time import sleep

def decode(data) -> str:
    ret = ""
    try:
        data = data.split(" ")
        for b in data:
            if ((b == "\n") or (b == "")):
                continue
            ret += chr(int(b, 16))
    except Exception as e:
        ret = None
    finally:
        return ret

def hexdump(data):
    def parse_ascii(b):
        ret = "."
        try:
            b = chr(b)
            if (b in string.printable):
                if ((b != "\n") and (b != "\r") and (b != "\t")):
                    ret = b
        except:
            pass
        return ret

    c = 0
    current = ""
    for _byte in data:
        b = str(hex(_byte)).replace("0x", "")
        if (len(b) == 1):
            b = "0" + b
        sys.stdout.write("%s " % b)
        current += parse_ascii(_byte)
        if (c == 7):
            sys.stdout.write("| %s\n" % current)
            sys.stdout.flush()
            current = ""
            c = 0
        else:
            c += 1
    if (c > 0):
        if (c == 7):
            sys.stdout.write("   ")
        else:
            c = 8 - c
            while (c != 0):
                sys.stdout.write("   ")
                c -= 1
        sys.stdout.write("| %s\n" % current)
        sys.stdout.flush()

class tap:
    def __init__(self, rport, lport, rhost, lhost="127.0.0.1", 
                 proto=SOCK_STREAM,
                 win_size=4096,
                 tls=False, tls_key_private=False,
                 tls_key_public=False, threads=20, callback=None,
                 backlog=1):
        self.running = False
        self.threads = []
        self.lsock = None

        self.backlog = backlog
        self.win_size = win_size
        self.proto = proto
        self.lport = lport
        self.rport = rport
        self.lhost = lhost
        self.rhost = rhost
        # TODO: implement ssl/tls usage 
        self.use_tls = tls
        # TODO: implement preset/user provided private/public key usage
        self.tls_privkey = tls_key_public
        self.tls_pubkey = tls_key_private
        self.max_threads = threads

        if (callback != None):
            self.callback = callback
        else:
            self.log("No callback function defined, not intercepting")
            self.callback = tap.callback_default
        
    # Bind self.lhost:self.lport for us
    #
    def bind(self):
        try:
            self.lsock = socket(AF_INET, self.proto, 0)
            self.lsock.bind((self.lhost, self.lport))
            self.lsock.listen(self.backlog)
        except Exception as err:
            self.fatal(err)

    # Connect to remote end for this connection, return connected socket
    # on success or None on error
    #
    def conn(self) -> object:
        sock = None
        try:
            sock = socket(AF_INET, self.proto, 0)
            sock.connect((self.rhost, self.rport))
        except Exception as err:
            error(err)
        finally:
            return sock

    # Log fatal exception, send quit signal, and wait for threads to 
    # be terminated
    #
    def fatal(self, msg):
        print("[Fatal] %s, quiting..." % msg)
        self.quit()

    # Log non-fatal error
    #
    def error(self, msg):
        print("[-] %s" % msg)

    def log(self, msg):
        print("[*] %s" % msg)

    # Wait until socket is readable, and return data received from it
    # on success, or None on error
    #
    def rx(self, sock, timeout=None) -> bytes:
        ret = None
        try:
            readable = select.select([sock], [], [], timeout)
            if (len(readable[0]) != 0):
                ret = sock.recv(self.win_size)
        except:
            pass
        finally:
            return ret

    # Try to transmit data, return amount of bytes sent on success or
    # 0 on error
    #
    def tx(self, sock, data, timeout=None) -> int:
        ret = 0
        try:
            writable = select.select([], [sock], [], timeout)
            if (len(writable[1]) != 0):
                ret = sock.send(data)
        except:
            pass
        finally:
            return ret

    # relay data from connection A to connection B via modifier function
    #
    def relay(self, conn_in, conn_out):
        while (self.running):
            # XXX: Should we have timeout? select isn't blocking, right?
            data = self.rx(conn_in, timeout=None)
            data = self.callback(data)
            if (type(data) == str):
                data = data.encode()
            if (data != None):
                stat = 0
                while (stat != len(data)):
                    if (self.running == False):
                        break
                    stat = self.tx(conn_out, data, timeout=None)
                    # Retry until we've sent all the data
                    data = data[stat:]
        try:
            conn_in.close()
        except Exception as err:
            self.log("Failed to close inbound connection: %s" % err)
            pass
        try:
            conn_out.close()
        except Exception as err:
            self.log("Failed to close outbound connection: %s" % err)
            pass

    # Handle inbound connection, connect to remote peer and
    # pass both connections to connection handler
    #
    # IF connecting to remote end fails, close inbound connection and
    # return.
    #
    def connect_sockets(self, cin):
        self.log("Got new inbound connection, attaching to remote host...")
        cout = self.conn()
        if (cout == None):
            cin.close()
            return
        t1 = threading.Thread(target=self.relay, args=(cin, cout,))
        t2 = threading.Thread(target=self.relay, args=(cout, cin,))
        self.threads.append(t1)
        self.threads.append(t2)
        t1.start()
        t2.start()

    # Listen for inbound connections from target, and handle them
    # 
    def run(self):
        self.running = True
        self.bind()
        tui = threading.Thread(target=self.tui)
        self.threads.append(tui)
        if (self.running == True):
            self.log("Started proxying...")
            self.lsock.settimeout(0.1)
        while (self.running):
            try:
                conn, addr = self.lsock.accept()
                self.log("Got connection from %s" % str(addr))
                self.connect_sockets(conn)
            except KeyboardInterrupt:
                self.quit()
            except TimeoutError:
                pass
            except:
                pass
    
    # User interface Aka ctrl-C handler
    #
    def tui(self):
        while (self.running):
            try:
                sleep(1)
            except KeyboardInterrupt:
                self.log("Got interrupted, quiting, please wait a second or few") 
                self.quit()

    # Set running to False, and wait for threads to be terminated
    #
    def quit(self):
        self.running = False
        for t in self.threads:
            try:
                t.join()
            except:
                pass

    # Default 'callback' function
    #
    def callback_default(data=None):
        if (not data):
            return ""
        return data

    # Manual interception of data
    #
    def callback_intercept(data=None):
        if (not data):
            return ""
        print("=" * 78)
        hexdump(data)
        print("=" * 78)
        try:
            r = input("Do you want to edit data above? y/N ")
        except KeyboardInterrupt:
            print("No need to be rude about it.. quiting now...")
            self.quit()
            return data
        if (("y" in r) or ("Y" in r)):
            print("Type bytes you want to replace data with as ascii-hex")
            print("Ie: 41 42 43 44 for ABCD")
            print("End with empty line.")
            user_data = ""
            while (True):
                try:
                    b = input("")
                except KeyboardInterrupt:
                    # Just return the data now,  user can ctrl-C again if they wish
                    # 
                    return data
                if (b == ""):
                    break
                else:
                    user_data += b
            data = decode(user_data)
            if (data == None):
                print("[-] Failed to decode user-provided data")
                data = ""
        return data

def cbhelp():
    print("Available callback functions:")
    print("\tdefault:         Pass data through without alterations")
    print("\tintercept:       Interactive interception")
    print("\tcustom:          Use custom intercepting function, see README")

def main():
    parser = argparse.ArgumentParser(
            description="Intercepting network proxy."
            )
    parser.add_argument(
            "--rport",
            type=int,
            help="Remote port to connect to."
            )
    parser.add_argument(
            "--rhost",
            type=str,
            help="Remote address to connect to."
            )
    parser.add_argument(
            "--lport",
            type=int,
            help="Local port to bind."
            )
    parser.add_argument(
            "--lhost",
            type=str,
            help="Local address to bind to, defaults to '127.0.0.1'.",
            default="127.0.0.1"
            )
    parser.add_argument(
            "--proto",
            type=str,
            help="Protocol to use, UDP or TCP, defaults to TCP.",
            default="TCP"
            )
    parser.add_argument(
            "--ws",
            type=int,
            help="How many bytes to read at once, defaults to 4096",
            default=4096
            )
    parser.add_argument(
            "--ssl",
            type=bool,
            help="Use ssl tunneling, true/false, defaults to false",
            default=False
            )
    # TODO: Add possibility to provide ssl keys
    parser.add_argument(
            "--mt",
            type=int,
            help="How many concurrent connections we support, defaults to 20",
            default=20
            )
    parser.add_argument(
            "--callback",
            help="Callback function to use, use --cbhelp for more info",
            default=None
            )
    parser.add_argument(
            "--cbhelp",
            help="Show callback function usage and quit",
            action='store_true'
            )
    parser.add_argument(
            "--backlog",
            type=int,
            help="How many connections we'll allow on backlog, defaults to 1.",
            default=1
            )
    args = parser.parse_args()

    if (args.proto == "TCP"):
        proto = SOCK_STREAM
    elif (args.proto == "UDP"):
        proto = SOCK_DGRAM
    else:
        print("Unknown protocol %s specified, only TCP and UDP are supported." % 
              args.proto)
        return
    if (args.cbhelp):
        cbhelp()
        return
    if ((not args.rhost) or (not args.rport) or (not args.lport)):
        print("Missing required parameters, see %s -h for usage" % sys.argv[0])
        return
    if (args.callback is not None):
        if ("interactive" in args.callback):
            cb = tap.callback_intercept
    else:
        cb = None
    t = tap(args.rport, args.lport, args.rhost, args.lhost, proto=proto,
            win_size=args.ws, tls=args.ssl, threads=args.mt, callback=cb, 
            backlog=args.backlog)

    t.run()

main()
