import json
from pydot import Dot #, Node, Edge
from sets import Set
import pydot

file = "audit.log.json"

dot = Dot(graph_type='digraph',fontname="Verdana")
data = json.loads(open(file).read())



# for now: one level of indirection

class Node:
    def __init__(self, kind, name, label):
        self.name = name
        self.kind = kind
        self.label = label

    def __str__(self):
        return self.kind + ": " + self.name

    def __hash__(self):
        return hash((self.kind, self.name))


class Entity(Node):
    def __init__(self, name, label):
        Node.__init__(self, "entity", name, label)


class Activity(Node):
    def __init__(self, name, label):
        Node.__init__(self, "activity", name, label)

class Edge:
    def __init__(self, kind, frm, to, label):
        self.kind = kind
        self.frm = frm
        self.to = to
        self.label = label
    
    def __cmp__(self, other):
        #print "CMP CALLED"
        if self.kind == other.kind and self.frm == other.frm and self.to == other.to: # and self.label == other.label:
            return 1
        else:
            return 0

    def __hash__(self):
        #return hash((self.frm, self.to, str(self.label)))
        return hash((self.kind, self.frm, self.to))
    
    def __str__(self):
        return self.kind + ": " + str(self.frm) + " --> " + str(self.to)

class Derived(Edge):
    def __init__(self, frm, to, label):
        Edge.__init__(self, "derived", frm, to, label)


class Generated(Edge):
    def __init__(self, frm, to, label):
        Edge.__init__(self, "generated", frm, to, label)

class Informed(Edge):
    def __init__(self, frm, to, label):
        Edge.__init__(self, "informed", frm, to, label)

class Used(Edge):
    def __init__(self, frm, to, label):
        Edge.__init__(self, "used", frm, to, label)

#class XC(Edge):
#    def __init__(self, frm, to, label, hops)
#        self.hops = hops
#        Edge.__init__(self, "XC", frm, to, label)


class ProvGraph:
    def __init__(self):
        self.nodes = Set()
        self.edges = Set()
        self.names = {}
        self.indx = {}

        self.source = Node("Source", "Source", {})

    def add_node(self, node):
        self.nodes.add(node)

        #print "register node named " + node.name
        self.names[node.name] = node

    def add_edge(self, edge):
        self.edges.add(edge)
        if edge.frm in self.indx:
            self.indx[edge.frm].add(edge.to)
        else:
            self.indx[edge.frm] = Set([edge.to])
      

    def node_named(self, name):
        if name in self.names:
            return self.names[name]
        else:
            return self.source

    def edge_at(self, name):
        if name in self.indx:
            return self.indx[name]
        else:
            return Set()
        

    def simplabel(self, node):
        if "prov:type" in node.label and node.label["prov:type"] == "packet":
            return node.label["prov:label"]
        else:
            return ""

    def draw(self):
        dt = Dot(graph_type='digraph',fontname="Verdana")
        for node in self.nodes:
            dt.add_node(pydot.Node(node.name, label=self.simplabel(node)))
        for edge in self.edges:
            dt.add_edge(pydot.Edge(edge.frm.name, edge.to.name))
        return dt


    def paths(self):
        new_edges = []
        for edge in self.edges:
            if "prov:type" in edge.frm.label and edge.frm.label["prov:type"] == "packet":
                print "TO " + str(edge.frm.label["prov:label"])
                #reachables = self.reachable(edge.to, Set())
                reachables = self.reachable(edge.to)
                #print "REACH " + str(reachables)
                for r in reachables:
                    #new_edges.append([edge.frm.label["prov:label"], r.label["prov:label"]])
                    new_edges.append([edge.frm, r])

        return new_edges


    def reachable(self, node):
        running = Set()
        for reach in self.edge_at(node):
            if "prov:type" in reach.label and reach.label["prov:type"] == "packet":
                print "RCH : " + reach.label["prov:label"]
                running.add(reach)
            else:
                running = running.union(self.reachable(reach))
             
             

        return running
        

    def paths2(self):
        # naive transitive closure
        my_edges = Set()
        deltas = Set()
        for edge in self.edges:
            my_edges.add(edge)
            deltas.add(edge)
            #if edge.frm.label["prov:type"] == "packet":
            #   print "PACKET! " + str(edge.frm.label) 

        print "OK"
        # compute to fixed point (naive evaluation)

        

    
        while (len(deltas) > 0):
            print "len is " + str(len(deltas))
            new_deltas = Set()
            for edge in my_edges:
                for de in deltas:
                    if edge.to == de.frm:
                        #print "MATCH " + str(edge.to) + " and " + str(de.frm)
                        #print "add edge " + str(edge.frm) + str(de.to)
                        new_deltas.add(Edge("XC", edge.frm, de.to, {}))
            print "NDL " + str(len(new_deltas))
            deltas = new_deltas.difference(my_edges)
            print "DIFF " + str(len(deltas))

    
            my_edges.update(deltas)
            print "ME " + str(len(my_edges))

            # now there better be a diff!
            dff = new_deltas.difference(my_edges)
            print "NEWDIFF is " + str(len(dff))

            for e in my_edges:
                print "EDGE is " + str(e)


        



# for later.
# name => labels
entities = {
    "entity": ["prov:label", "prov:type"],
    "activity": ["prov:label"]
}

# name => [from, to, labels]
relations = {
    #"wasDerivedFrom": ["
}

entities = data["entity"]
derived = data["wasDerivedFrom"]
generated = data["wasGeneratedBy"]
activities = data["activity"]
informs = data["wasInformedBy"]
uses = data["used"]

graph = ProvGraph()




# test cases.

#graph.add_node(Entity("foo", {}))
#graph.add_node(Entity("bar", {}))
#graph.add_node(Entity("baz", {}))
#graph.add_node(Entity("qux", {}))

#graph.add_edge(Derived(graph.node_named("foo"), graph.node_named("bar"), {}))
#graph.add_edge(Derived(graph.node_named("bar"), graph.node_named("baz"), {}))
#graph.add_edge(Derived(graph.node_named("baz"), graph.node_named("qux"), {}))

#graph.add_edge(Derived(graph.node_named("qux"), graph.node_named("foo"), {}))


#graph.paths()


#exit()



for entity in activities.keys():
    graph.add_node(Activity(entity, activities[entity]))

for entity in entities.keys():
    labels = []
    if "prov:type" in entities[entity]:
        labels.append(entities[entity]["prov:type"])
    if "prov:label" in entities[entity]:
        labels.append(entities[entity]["prov:label"])
    dot.add_node(pydot.Node(entity, label = "|".join(labels), shape = "box"))
    graph.add_node(Entity(entity, entities[entity]))


for entity in derived.keys():
    #print "COMPARE this " + entity + " vs " + derived[entity]["prov:generatedEntity"]
    dot.add_edge(pydot.Edge(derived[entity]["prov:usedEntity"], derived[entity]["prov:generatedEntity"], label="DERIVED: " + derived[entity]["prov:label"]))
    graph.add_edge(Derived(graph.node_named(derived[entity]["prov:usedEntity"]), graph.node_named(derived[entity]["prov:generatedEntity"]), derived[entity]))
    
for entity in generated.keys():
    #print "YO " + str(generated[entity])
    #dot.add_edge(pydot.Edge(generated[entity]["prov:activity"], generated[entity]["prov:entity"], label = "GENERATED: " + generated[entity]["prov:label"]))
    dot.add_edge(pydot.Edge(generated[entity]["prov:entity"], generated[entity]["prov:activity"], label = "GENERATED: " + generated[entity]["prov:label"]))
    #graph.add_edge(Generated(graph.node_named(generated[entity]["prov:activity"]), graph.node_named(generated[entity]["prov:entity"]), generated[entity]))
    graph.add_edge(Generated(graph.node_named(generated[entity]["prov:entity"]), graph.node_named(generated[entity]["prov:activity"]), generated[entity]))

for inform in informs.keys():
    dot.add_edge(pydot.Edge(informs[inform]["prov:informant"], informs[inform]["prov:informed"], label = "INFORMED: " + informs[inform]["prov:label"]))
    graph.add_edge(Informed(graph.node_named(informs[inform]["prov:informant"]), graph.node_named(informs[inform]["prov:informed"]), informs[inform]))

# to suppress the many duplicates in USES
memo = {}
for use in uses.keys():
    ky = uses[use]["prov:entity"] +  uses[use]["prov:activity"] 
    if ky not in memo:
        dot.add_edge(pydot.Edge(uses[use]["prov:entity"], uses[use]["prov:activity"], label = "USES: " + uses[use]["prov:label"]))
        graph.add_edge(Used(graph.node_named(uses[use]["prov:entity"]), graph.node_named( uses[use]["prov:activity"]),  uses[use]))
    memo[ky] = True


dot.write_raw("foo")

#edgs = graph.paths()
#dt = Dot(graph_type='digraph',fontname="Verdana")

#for e in edgs:
#    dt.add_node(pydot.Node(e[0].name, label = e[0].label["prov:label"]))
#    dt.add_node(pydot.Node(e[1].name, label = e[1].label["prov:label"]))

#for e in edgs:
#    dt.add_edge(pydot.Edge(e[0].name, e[1].name))

#dt.write_raw("bar")


dt2 = graph.draw()
dt2.write_raw("baz")


# then run "dot -Tpdf foo > foo.pdf"
