import json
from pydot import Dot #, Node, Edge
from sets import Set
import pydot

file = "audit.log.json"

dot = Dot(graph_type='digraph',fontname="Verdana")
data = json.loads(open(file).read())


entities = data["entity"]
if "wasDerivedFrom" in data:
    derived = data["wasDerivedFrom"]
if "wasGeneratedBy" in data:
    generated = data["wasGeneratedBy"]
activities = data["activity"]
if "wasInformedBy" in data:
    informs = data["wasInformedBy"]
uses = data["used"]


# what a huge pain in the ass

class Mapping:
    def __init__(self):
        self.mapping = {}
        self.seq = 0

    def mapped(self, bid):
        if bid in self.mapping:
            ret = self.mapping[bid]
        else:
            self.mapping[bid] = self.seq
            self.seq += 1
            ret = self.seq-1

        return "n" + str(ret)


def entitystr(json, mapping):
    # removing the "=" in a base64-encoded bytestring may end up being problematic!
    #return json.encode("utf-8").replace(":", "_").replace("=", "")
    return mapping.mapped(json)

def keystr(json):
    # lol "keystr"
    return json.encode("utf-8").replace(":", "_")

def unp(json):
    if type(json) is dict:
        #return neo_ready(json)
        # nest it
        nested = neo_ready(json)
        return "\'" + nested.replace("'", "\\'") + "\'"
    else:
        return "\'" + str(json) + "\'"


def neo_ready(json):
    ret = '{'
    ret += ", ".join(map(lambda k: keystr(k) + ": " + unp(json[k]), json.keys()))
    ret += "}"
    return ret

def clern(jsn):
    ret = jsn
    for t in [",", "-", ">", "[", "]", ".", ":", " ", ")", "(", "/", ';', '%', '*', '}', '{', '$', '~', '`', '!', '@', '#', '^', '&', '*', '?', '=', '\\', '"', '|']:
        ret = ret.replace(t, "_")
    return ret

def label(entity, json):
    if u'prov:label' in json:
        return clern(json[u'prov:label'])
    else:
        #return entity
        return "blank"



mapping = Mapping()

for entity in entities.keys():
    print "CREATE (" + entitystr(entity, mapping) + ": " + label(entity, entities[entity]) + " "  + neo_ready(entities[entity]) + ")"

for activity in activities.keys():
    print "CREATE (" + entitystr(activity, mapping) + ": " + label(activity, activities[activity]) + " "  + neo_ready(activities[activity]) + ")"

if "wasDerivedFrom" in data:
    for entity in derived.keys():
        print "CREATE (" + entitystr(derived[entity]["prov:usedEntity"], mapping) +") -[" + entitystr(entity, mapping) + ": Derived " + neo_ready(derived[entity]) + "]-> (" + entitystr(derived[entity]["prov:generatedEntity"], mapping) + ")" 

if "wasGeneratedBy" in data:
    for entity in generated.keys():
        print "CREATE (" + entitystr(generated[entity]["prov:activity"], mapping) +") -[" + entitystr(entity, mapping) + ": Generated " + neo_ready(generated[entity]) + "]-> (" + entitystr(generated[entity]["prov:entity"], mapping) + ")" 

for entity in uses.keys():
    print "CREATE (" + entitystr(uses[entity]["prov:activity"], mapping) +") -[" + entitystr(entity, mapping) + ": Uses " + neo_ready(uses[entity]) + "]-> (" + entitystr(uses[entity]["prov:entity"], mapping) + ")" 

#for entity in entities.keys():
#    labels = []
#    if "prov:type" in entities[entity]:
#        labels.append(entities[entity]["prov:type"])
#    if "prov:label" in entities[entity]:
#        labels.append(entities[entity]["prov:label"])
#    dot.add_node(pydot.Node(entity, label = "|".join(labels), shape = "box"))
#    graph.add_node(Entity(entity, entities[entity]))


#for entity in derived.keys():
#    #print "COMPARE this " + entity + " vs " + derived[entity]["prov:generatedEntity"]
#    dot.add_edge(pydot.Edge(derived[entity]["prov:usedEntity"], derived[entity]["prov:generatedEntity"], label="DERIVED: " + derived[entity]["prov:label"]))
#    graph.add_edge(Derived(graph.node_named(derived[entity]["prov:usedEntity"]), graph.node_named(derived[entity]["prov:generatedEntity"]), derived[entity]))
    
#for entity in generated.keys():
#    #print "YO " + str(generated[entity])
#    #dot.add_edge(pydot.Edge(generated[entity]["prov:activity"], generated[entity]["prov:entity"], label = "GENERATED: " + generated[entity]["prov:label"]))
#    dot.add_edge(pydot.Edge(generated[entity]["prov:entity"], generated[entity]["prov:activity"], label = "GENERATED: " + generated[entity]["prov:label"]))
#    #graph.add_edge(Generated(graph.node_named(generated[entity]["prov:activity"]), graph.node_named(generated[entity]["prov:entity"]), generated[entity]))
#    graph.add_edge(Generated(graph.node_named(generated[entity]["prov:entity"]), graph.node_named(generated[entity]["prov:activity"]), generated[entity]))

#for inform in informs.keys():
#    dot.add_edge(pydot.Edge(informs[inform]["prov:informant"], informs[inform]["prov:informed"], label = "INFORMED: " + informs[inform]["prov:label"]))
#    graph.add_edge(Informed(graph.node_named(informs[inform]["prov:informant"]), graph.node_named(informs[inform]["prov:informed"]), informs[inform]))

## to suppress the many duplicates in USES
#memo = {}
#for use in uses.keys():
#    ky = uses[use]["prov:entity"] +  uses[use]["prov:activity"] 
#    if ky not in memo:
#        dot.add_edge(pydot.Edge(uses[use]["prov:entity"], uses[use]["prov:activity"], label = "USES: " + uses[use]["prov:label"]))
#        graph.add_edge(Used(graph.node_named(uses[use]["prov:entity"]), graph.node_named( uses[use]["prov:activity"]),  uses[use]))
#    memo[ky] = True


