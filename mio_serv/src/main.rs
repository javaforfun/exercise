extern crate mio;
extern crate slab;

use mio::{PollOpt, Ready, Token};
use mio::deprecated::{EventLoop, Handler};
use mio::tcp::{TcpListener, TcpStream};
use std::io;
use std::io::{Read, Write};

const SERVER: mio::Token = mio::Token(10_000_000);

struct Conn {
    socket: TcpStream,
    recv_buf: Vec<u8>,
    send_buf: Vec<u8>,
    token: Option<Token>,
    interest: Ready,
    closed: bool
}

type Slab<T> = slab::Slab<T, Token>;

impl Conn {
    fn new(socket: TcpStream) -> Conn {
        Conn {
            socket: socket,
            recv_buf: Vec::with_capacity(1024),
            send_buf: Vec::with_capacity(1024),
            token: None,
            interest: Ready::writable(),
            closed: false
        }
    }

    fn writable(&mut self, event_loop: &mut EventLoop<Pong>) -> io::Result<()> {
        match self.socket.write(&self.send_buf) {
            Ok(r) if r < self.send_buf.len() => {
                println!("client flushing buf; write {} bytes", r);
                if r > 0 {
                    let vec2 = self.send_buf.split_off(r);
                    self.send_buf.clone_from(&vec2);
                }
                self.writable(event_loop);
                return Ok(())
            }
            Ok(r) => {
                println!("conn: we wrote {} bytes", r);
                self.send_buf.clear();
                self.interest.remove(Ready::writable());
            }
            Err(ref e) if e.kind() == io::ErrorKind::WouldBlock => {
                println!("client flushing buf");
                self.interest.insert(Ready::writable());
            }
            Err(e) => {
                println!("write err={:?}", e);
                return Err(e)
            }
        }

        event_loop.reregister(&self.socket, self.token.unwrap(),
                              self.interest | Ready::readable() | Ready::hup(),
                              PollOpt::edge())
    }

    fn readable(&mut self, event_loop: &mut EventLoop<Pong>) {
        unsafe { self.recv_buf.set_len(1024); }
        match self.socket.read(&mut self.recv_buf) {
            Ok(0) => {
                println!("conn close");
                self.closed = true;
            }
            Ok(r) => {
                println!("conn: read {} bytes", r);
                self.recv_buf.truncate(r);
                self.send_buf.append(&mut self.recv_buf);
                self.writable(event_loop);
                self.readable(event_loop);
            }
            Err(ref e) if e.kind() == io::ErrorKind::WouldBlock => {
                println!("eagain");
            }
            Err(e) => {
                println!("read err {:?}", e);
                self.closed = true;
            }
        }
    }

    fn is_closed(&self) -> bool {
        self.closed
    }
}

struct Pong {
    server: TcpListener,
    conns: Slab<Conn>,
}

impl Pong {
    fn new(server: TcpListener) -> Pong {
        Pong {
            server: server,
            conns: Slab::with_capacity(100000)
        }
    }

    fn accept(&mut self, event_loop: &mut EventLoop<Pong>) -> io::Result<()> {
        println!("server accept socket");
        let sock = self.server.accept().unwrap().0;
        let conn = Conn::new(sock);
        let tok = self.conns.insert(conn).ok()
            .expect("could not add connection to slab");
        self.conns[tok].token = Some(tok);
        
        event_loop.register(&self.conns[tok].socket, tok,
                            Ready::readable() | Ready::hup(),
                            PollOpt::edge()).ok()
            .expect("could not register socket with event loop");
        println!("accept conn: {:?}", tok);
        Ok(())
    }

    fn conn_readable(&mut self, event_loop: &mut EventLoop<Pong>, tok: Token)
                     -> io::Result<()> {
        println!("server conn readable, tok={:?}", tok);
        self.conns[tok].readable(event_loop);
        if self.conns[tok].is_closed() {
            self.conns.remove(tok);
        }
        Ok(())
    }

    fn conn_writable(&mut self, event_loop: &mut EventLoop<Pong>, tok: Token)
                     -> io::Result<()> {
        println!("server conn writable, tok={:?}", tok);
        self.conns[tok].writable(event_loop)
    }
}

impl Handler for Pong {
    type Timeout = usize;
    type Message = ();

    fn ready(&mut self, event_loop: &mut EventLoop<Pong>, token: Token,
             events: Ready) {
        println!("ready {:?} {:?}", token, events);
        if events.is_hup() {
            println!("connection close, {:?}", token);
            self.conns.remove(token);
            return;
        }
        if events.is_readable() {
            match token {
                SERVER => self.accept(event_loop).unwrap(),
                i => self.conn_readable(event_loop, i).unwrap()
            }
        }
        if events.is_writable() {
            match token {
                SERVER => panic!("received writable for token 0"),
                _ => self.conn_writable(event_loop, token).unwrap()
            }
        }
    }
}

fn main() {
    let addr = "0.0.0.0:11021".parse().unwrap();
    let server = TcpListener::bind(&addr).unwrap();

    let mut event_loop = EventLoop::new().unwrap();
    event_loop.register(&server, SERVER, Ready::readable(), PollOpt::edge())
        .unwrap();

    let mut pong = Pong::new(server);

    println!("server start, addr: 0.0.0.0:11021");
    event_loop.run(&mut pong).unwrap();
}
