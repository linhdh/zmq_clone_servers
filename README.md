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

## Republishing Updates from Clients (clone pattern 3)

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

## Working with Subtrees (clone pattern 4)

As we grow the number of clients, the size of our shared store will also grow. It stops being reasonable to send everything to every client. This is the classic story with pub-sub: when you have a very small number of clients, you can send every message to all clients. As you grow the architecture, this becomes inefficient. Clients specialize in different areas.

So even when working with a shared store, some clients will want to work only with a part of that store, which we call a subtree. The client has to request the subtree when it makes a state request, and it must specify the same subtree when it subscribes to updates.

There are a couple of common syntaxes for trees. One is the path hierarchy, and another is the topic tree. These look like this:

- Path hierarchy: /some/list/of/paths
- Topic tree: some.list.of.topics

We’ll use the path hierarchy, and extend our client and server so that a client can work with a single subtree. Once you see how to work with a single subtree you’ll be able to extend this yourself to handle multiple subtrees, if your use case demands it.

## Ephemeral Values (clone pattern 5)

An ephemeral value is one that expires automatically unless regularly refreshed. If you think of Clone being used for a registration service, then ephemeral values would let you do dynamic values. A node joins the network, publishes its address, and refreshes this regularly. If the node dies, its address eventually gets removed.

The usual abstraction for ephemeral values is to attach them to a session, and delete them when the session ends. In Clone, sessions would be defined by clients, and would end if the client died. A simpler alternative is to attach a time to live (TTL) to ephemeral values, which the server uses to expire values that haven’t been refreshed in time.

A good design principle that I use whenever possible is to not invent concepts that are not absolutely essential. If we have very large numbers of ephemeral values, sessions will offer better performance. If we use a handful of ephemeral values, it’s fine to set a TTL on each one. If we use masses of ephemeral values, it’s more efficient to attach them to sessions and expire them in bulk. This isn’t a problem we face at this stage, and may never face, so sessions go out the window.

Now we will implement ephemeral values. First, we need a way to encode the TTL in the key-value message. We could add a frame. The problem with using ZeroMQ frames for properties is that each time we want to add a new property, we have to change the message structure. It breaks compatibility. So let’s add a properties frame to the message, and write the code to let us get and put property values.

Next, we need a way to say, “delete this value”. Up until now, servers and clients have always blindly inserted or updated new values into their hash table. We’ll say that if the value is empty, that means “delete this key”.

A more complete version of the kvmsg class, which implements the properties frame (and adds a UUID frame, which we’ll need later on). It also handles empty values by deleting the key from the hash.

## Adding the Binary Star Pattern for Reliability (clone pattern 6)

The Clone models we’ve explored up to now have been relatively simple. Now we’re going to get into unpleasantly complex territory, which has me getting up for another espresso. You should appreciate that making “reliable” messaging is complex enough that you always need to ask, “Do we actually need this?” before jumping into it. If you can get away with unreliable or with “good enough” reliability, you can make a huge win in terms of cost and complexity. Sure, you may lose some data now and then. It is often a good trade-off. Having said, that, and… sips… because the espresso is really good, let’s jump in.

As you play with the last model, you’ll stop and restart the server. It might look like it recovers, but of course it’s applying updates to an empty state instead of the proper current state. Any new client joining the network will only get the latest updates instead of the full historical record.

What we want is a way for the server to recover from being killed, or crashing. We also need to provide backup in case the server is out of commission for any length of time. When someone asks for “reliability”, ask them to list the failures they want to handle. In our case, these are:

- The server process crashes and is automatically or manually restarted. The process loses its state and has to get it back from somewhere.

- The server machine dies and is offline for a significant time. Clients have to switch to an alternate server somewhere.

- The server process or machine gets disconnected from the network, e.g., a switch dies or a datacenter gets knocked out. It may come back at some point, but in the meantime clients need an alternate server.

Our first step is to add a second server. We can use the Binary Star pattern from Chapter 4 - Reliable Request-Reply Patterns to organize these into primary and backup. Binary Star is a reactor, so it’s useful that we already refactored the last server model into a reactor style.

We need to ensure that updates are not lost if the primary server crashes. The simplest technique is to send them to both servers. The backup server can then act as a client, and keep its state synchronized by receiving updates as all clients do. It’ll also get new updates from clients. It can’t yet store these in its hash table, but it can hold onto them for a while.

So, Model Six introduces the following changes over Model Five:

- We use a pub-sub flow instead of a push-pull flow for client updates sent to the servers. This takes care of fanning out the updates to both servers. Otherwise we’d have to use two DEALER sockets.

- We add heartbeats to server updates (to clients), so that a client can detect when the primary server has died. It can then switch over to the backup server.

- We connect the two servers using the Binary Star bstar reactor class. Binary Star relies on the clients to vote by making an explicit request to the server they consider active. We’ll use snapshot requests as the voting mechanism.

- We make all update messages uniquely identifiable by adding a UUID field. The client generates this, and the server propagates it back on republished updates.

The passive server keeps a “pending list” of updates that it has received from clients, but not yet from the active server; or updates it’s received from the active server, but not yet from the clients. The list is ordered from oldest to newest, so that it is easy to remove updates off the head.

![Clone Client Finite State Machine](/images/fig61.png)

It’s useful to design the client logic as a finite state machine. The client cycles through three states:

The client opens and connects its sockets, and then requests a snapshot from the first server. To avoid request storms, it will ask any given server only twice. One request might get lost, which would be bad luck. Two getting lost would be carelessness.

- The client waits for a reply (snapshot data) from the current server, and if it gets it, it stores it. If there is no reply within some timeout, it fails over to the next server.

- When the client has gotten its snapshot, it waits for and processes updates. Again, if it doesn’t hear anything from the server within some timeout, it fails over to the next server.

The client loops forever. It’s quite likely during startup or failover that some clients may be trying to talk to the primary server while others are trying to talk to the backup server. The Binary Star state machine handles this, hopefully accurately. It’s hard to prove software correct; instead we hammer it until we can’t prove it wrong.

Failover happens as follows:

- The client detects that primary server is no longer sending heartbeats, and concludes that it has died. The client connects to the backup server and requests a new state snapshot.

- The backup server starts to receive snapshot requests from clients, and detects that primary server has gone, so it takes over as primary.

- The backup server applies its pending list to its own hash table, and then starts to process state snapshot requests.

When the primary server comes back online, it will:

- Start up as passive server, and connect to the backup server as a Clone client.

- Start to receive updates from clients, via its SUB socket.

We make a few assumptions:

- At least one server will keep running. If both servers crash, we lose all server state and there’s no way to recover it.

- Multiple clients do not update the same hash keys at the same time. Client updates will arrive at the two servers in a different order. Therefore, the backup server may apply updates from its pending list in a different order than the primary server would or did. Updates from one client will always arrive in the same order on both servers, so that is safe.

Thus the architecture for our high-availability server pair using the Binary Star pattern has two servers and a set of clients that talk to both servers.

![High-availability Clone Server Pair](/images/fig61.png)

This model is only a few hundred lines of code, but it took quite a while to get working. To be accurate, building Model Six took about a full week of “Sweet god, this is just too complex for an example” hacking. We’ve assembled pretty much everything and the kitchen sink into this small application. We have failover, ephemeral values, subtrees, and so on. What surprised me was that the up-front design was pretty accurate. Still the details of writing and debugging so many socket flows is quite challenging.

The reactor-based design removes a lot of the grunt work from the code, and what remains is simpler and easier to understand. We reuse the bstar reactor from Chapter 4 - Reliable Request-Reply Patterns. The whole server runs as one thread, so there’s no inter-thread weirdness going on–just a structure pointer (self) passed around to all handlers, which can do their thing happily. One nice side effect of using reactors is that the code, being less tightly integrated into a poll loop, is much easier to reuse. Large chunks of Model Six are taken from Model Five.

I built it piece by piece, and got each piece working properly before going onto the next one. Because there are four or five main socket flows, that meant quite a lot of debugging and testing. I debugged just by dumping messages to the console. Don’t use classic debuggers to step through ZeroMQ applications; you need to see the message flows to make any sense of what is going on.

For testing, I always try to use Valgrind, which catches memory leaks and invalid memory accesses. In C, this is a major concern, as you can’t delegate to a garbage collector. Using proper and consistent abstractions like kvmsg and CZMQ helps enormously.

## The Clustered Hashmap Protocol

While the server is pretty much a mashup of the previous model plus the Binary Star pattern, the client is quite a lot more complex. But before we get to that, let’s look at the final protocol. I’ve written this up as a specification on the ZeroMQ RFC website as the Clustered Hashmap Protocol.

Roughly, there are two ways to design a complex protocol such as this one. One way is to separate each flow into its own set of sockets. This is the approach we used here. The advantage is that each flow is simple and clean. The disadvantage is that managing multiple socket flows at once can be quite complex. Using a reactor makes it simpler, but still, it makes a lot of moving pieces that have to fit together correctly.

The second way to make such a protocol is to use a single socket pair for everything. In this case, I’d have used ROUTER for the server and DEALER for the clients, and then done everything over that connection. It makes for a more complex protocol but at least the complexity is all in one place. In Chapter 7 - Advanced Architecture using ZeroMQ we’ll look at an example of a protocol done over a ROUTER-DEALER combination.

Let’s take a look at the CHP specification. Note that “SHOULD”, “MUST” and “MAY” are key words we use in protocol specifications to indicate requirement levels.


