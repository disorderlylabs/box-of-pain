from neo4j.v1 import GraphDatabase, basic_auth


URI = "bolt://localhost:7687"
USERNAME = "neo4j"
PASSWORD = "neo"
INPUTGRAPH = "creates.nwo"

driver = GraphDatabase.driver(URI, auth=(USERNAME, PASSWORD))

with driver.session() as session:
    with session.begin_transaction() as tx:
        tx.run("match (n) detach delete n")
        s = open(INPUTGRAPH, 'r').read()
        tx.run(s)
