# ctf_pizzaservice

This was used at the (attack and defense) capture-the-flag event 7.2.2020 at HS Albstadt-Sigmaringen.

It is a webservice for ordering pizza and has some exploitable weaknesses for the competitors to find. Example exploits are provided.

Placing an order produces a JSON web token signed by server's rsa key. This token has to be provided to get the receipt. The goal is to get receipt information (->flag) of someone else. This can then be exchanged for points.


## Install

The backend is only tested with ubuntu 19.04 for wich I built a development container.
Either use VScode with the remote-containers plugin or build a container yourself from the provided dockerfile.
Container configuration is in ./.devcontainer/

Inside the container run:

```bash
./init.sh --new
./pizzaservice 7777 order.db rsa_keys/pub_key.pem rsa_keys/priv_key.pem
```

Don't forget to forward the port from the container.

For the exploits to work there has to be at least one order in the db:

```bash
curl -d pizza_id=1 -d pizza_id=2 -d name=somename -d address=someaddr 127.0.0.1:7777/order
```

The python exploit scripts need pip package ´pyjwt`. You can use pipenv (Pipfile provided):

```bash
pipenv shell
pipenv update
python3 exploit_client_signed_token.py 127.0.0.1 7777
```

The .sh exploit scripts need httpie: https://github.com/jakubroztocil/httpie


## Exploit description

### debugmode
GETting /receipt with debug query parameter and an invalid Json Web token (there has to be one) results in skipped token verification

### path traversal
GET /../order.db or GET /../rsa_keys/priv_key.pem are possible. With this info one could just read the flags out of the db or forge valid tokens

### nonealg
The JWT standard contains the {"alg": "None"} algorithm for signing the token. Verifying a token with "None" algorithm always succeeds. This is implemented by explicitly ignoring an error set by the jwt implementation when verifying a "None"-signature

### client signed token
The Server trusts the token before validating its signature by reading the algorithm to use verification out of the token instead of just using rsa. 
The token is signed asymmetrically with the private key of the server. A client can get the public key of the server and forge a symmetrically signed token with the pubkey as key. The server will then verify the token using its public key and the symmetrical algorithm wich leads to successful verification.