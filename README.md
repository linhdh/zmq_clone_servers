# Reliable Pub-Sub (Clone Pattern)

As a larger worked example, we’ll take the problem of making a reliable pub-sub architecture. We’ll develop this in stages. The goal is to allow a set of applications to share some common state. Here are our technical challenges:

- We have a large set of client applications, say thousands or tens of thousands.
- They will join and leave the network arbitrarily.
- These applications must share a single eventually-consistent state.
- Any application can update the state at any point in time.

Let’s say that updates are reasonably low-volume. We don’t have real time goals. The whole state can fit into memory. Some plausible use cases are:

- A configuration that is shared by a group of cloud servers.
- Some game state shared by a group of players.
- Exchange rate data that is updated in real time and available to applications.

## Centralized Versus Decentralized

A first decision we have to make is whether we work with a central server or not. It makes a big difference in the resulting design. The trade-offs are these:

- Conceptually, a central server is simpler to understand because networks are not naturally symmetrical. With a central server, we avoid all questions of discovery, bind versus connect, and so on.

- Generally, a fully-distributed architecture is technically more challenging but ends up with simpler protocols. That is, each node must act as server and client in the right way, which is delicate. When done right, the results are simpler than using a central server. We saw this in the Freelance pattern in Chapter 4 - Reliable Request-Reply Patterns.

- A central server will become a bottleneck in high-volume use cases. If handling scale in the order of millions of messages a second is required, we should aim for decentralization right away.

- Ironically, a centralized architecture will scale to more nodes more easily than a decentralized one. That is, it’s easier to connect 10,000 nodes to one server than to each other.

So, for the Clone pattern we’ll work with a server that publishes state updates and a set of clients that represent applications.

## Representing State as Key-Value Pairs

We’ll develop Clone in stages, solving one problem at a time. First, let’s look at how to update a shared state across a set of clients. We need to decide how to represent our state, as well as the updates. The simplest plausible format is a key-value store, where one key-value pair represents an atomic unit of change in the shared state.

We have a simple pub-sub example in Chapter 1 - Basics, the weather server and client. Let’s change the server to send key-value pairs, and the client to store these in a hash table. This lets us send updates from one server to a set of clients using the classic pub-sub model.

An update is either a new key-value pair, a modified value for an existing key, or a deleted key. We can assume for now that the whole store fits in memory and that applications access it by key, such as by using a hash table or dictionary. For larger stores and some kind of persistence we’d probably store the state in a database, but that’s not relevant here.

![Publishing State Updates](/images/fig58.png)

Here are some things to note about this first model:

- All the hard work is done in a kvmsg class. This class works with key-value message objects, which are multipart ZeroMQ messages structured as three frames: a key (a ZeroMQ string), a sequence number (64-bit value, in network byte order), and a binary body (holds everything else).

- The server generates messages with a randomized 4-digit key, which lets us simulate a large but not enormous hash table (10K entries).

- We don’t implement deletions in this version: all messages are inserts or updates.

- The server does a 200 millisecond pause after binding its socket. This is to prevent slow joiner syndrome, where the subscriber loses messages as it connects to the server’s socket. We’ll remove that in later versions of the Clone code.

- We’ll use the terms publisher and subscriber in the code to refer to sockets. This will help later when we have multiple sockets doing different things.

Both the server and client maintain hash tables, but this first model only works properly if we start all clients before the server and the clients never crash. That’s very artificial.

## Getting an Out-of-Band Snapshot

So now we have our second problem: how to deal with late-joining clients or clients that crash and then restart.

In order to allow a late (or recovering) client to catch up with a server, it has to get a snapshot of the server’s state. Just as we’ve reduced “message” to mean “a sequenced key-value pair”, we can reduce “state” to mean “a hash table”. To get the server state, a client opens a DEALER socket and asks for it explicitly.

To make this work, we have to solve a problem of timing. Getting a state snapshot will take a certain time, possibly fairly long if the snapshot is large. We need to correctly apply updates to the snapshot. But the server won’t know when to start sending us updates. One way would be to start subscribing, get a first update, and then ask for “state for update N”. This would require the server storing one snapshot for each update, which isn’t practical.

![State Replication](/images/fig59.png)

So we will do the synchronization in the client, as follows:

- The client first subscribes to updates and then makes a state request. This guarantees that the state is going to be newer than the oldest update it has.

- The client waits for the server to reply with state, and meanwhile queues all updates. It does this simply by not reading them: ZeroMQ keeps them queued on the socket queue.

- When the client receives its state update, it begins once again to read updates. However, it discards any updates that are older than the state update. So if the state update includes updates up to 200, the client will discard updates up to 201.

- The client then applies updates to its own state snapshot.

It’s a simple model that exploits ZeroMQ’s own internal queues.

Here are some things to note about these two programs (clonesrv2, clonecli2):

- The server uses two tasks. One thread produces the updates (randomly) and sends these to the main PUB socket, while the other thread handles state requests on the ROUTER socket. The two communicate across PAIR sockets over an inproc:@<//>@ connection.

- The client is really simple. In C, it consists of about fifty lines of code. A lot of the heavy lifting is done in the kvmsg class. Even so, the basic Clone pattern is easier to implement than it seemed at first.

- We don’t use anything fancy for serializing the state. The hash table holds a set of kvmsg objects, and the server sends these, as a batch of messages, to the client requesting state. If multiple clients request state at once, each will get a different snapshot.

- We assume that the client has exactly one server to talk to. The server must be running; we do not try to solve the question of what happens if the server crashes.

Right now, these two programs don’t do anything real, but they correctly synchronize state. It’s a neat example of how to mix different patterns: PAIR-PAIR, PUB-SUB, and ROUTER-DEALER.

## Republishing Updates from Clients

In our second model, changes to the key-value store came from the server itself. This is a centralized model that is useful, for example if we have a central configuration file we want to distribute, with local caching on each node. A more interesting model takes updates from clients, not the server. The server thus becomes a stateless broker. This gives us some benefits:

- We’re less worried about the reliability of the server. If it crashes, we can start a new instance and feed it new values.

- We can use the key-value store to share knowledge between active peers.

To send updates from clients back to the server, we could use a variety of socket patterns. The simplest plausible solution is a PUSH-PULL combination.

Why don’t we allow clients to publish updates directly to each other? While this would reduce latency, it would remove the guarantee of consistency. You can’t get consistent shared state if you allow the order of updates to change depending on who receives them. Say we have two clients, changing different keys. This will work fine. But if the two clients try to change the same key at roughly the same time, they’ll end up with different notions of its value.

There are a few strategies for obtaining consistency when changes happen in multiple places at once. We’ll use the approach of centralizing all change. No matter the precise timing of the changes that clients make, they are all pushed through the server, which enforces a single sequence according to the order in which it gets updates.

![Republishing Updates](/images/fig60.png)

By mediating all changes, the server can also add a unique sequence number to all updates. With unique sequencing, clients can detect the nastier failures, including network congestion and queue overflow. If a client discovers that its incoming message stream has a hole, it can take action. It seems sensible that the client contact the server and ask for the missing messages, but in practice that isn’t useful. If there are holes, they’re caused by network stress, and adding more stress to the network will make things worse. All the client can do is warn its users that it is “unable to continue”, stop, and not restart until someone has manually checked the cause of the problem.

We’ll now generate state updates in the client.

- The server has collapsed to a single task. It manages a PULL socket for incoming updates, a ROUTER socket for state requests, and a PUB socket for outgoing updates.

- The client uses a simple tickless timer to send a random update to the server once a second. In a real implementation, we would drive updates from application code.




