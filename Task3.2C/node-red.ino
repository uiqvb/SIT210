if (msg.payload !== "Manit") {
    return null;
}

let count = flow.get("waveCount") || 0;
count = count + 1;
flow.set("waveCount", count);

node.status({ fill: "blue", shape: "dot", text: "count = " + count });

if (count >= 3) {
    msg.topic = "SIT730 Alert: 3 waves detected";
    msg.payload =
        "Hello Carer,\n\n" +
        "3 wave gestures have been detected from Manit.\n" +
        "Please check on Linda.\n\n" +
        "Sent automatically from Node-RED.";

    flow.set("waveCount", 0);
    node.status({ fill: "green", shape: "dot", text: "email sent, reset to 0" });

    return msg;
}

return null;