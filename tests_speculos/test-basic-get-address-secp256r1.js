'use strict';

import * as common from './common.js';
import { default as OnflowLedgerMod } from "@onflow/ledger";
import { fileURLToPath } from 'url';

var scriptName = common.path.basename(fileURLToPath(import.meta.url));

common.testStart(scriptName);

const FlowApp = OnflowLedgerMod.default;
const app = new FlowApp(common.mockTransport);

console.log(common.humanTime() + " // using FlowApp below with common.mockTransport() to grab apdu command without sending it");
const slot = 2;
const address = "e467b9dd11fa00df"
const scheme = FlowApp.Signature.P256 | FlowApp.Hash.SHA2_256;
const path = `m/44'/539'/${scheme}'/0/0`;

console.log(common.humanTime() + " // screen shot before sending first apdu command");
common.curlScreenShot(scriptName);

common.testStep(" - - -", "await app.setSlot() // slot=" + slot  + " address=" + address + " path=" + path);
await app.setSlot(slot , address, path);
var hexOutgoing = common.hexApduCommandViaMockTransportArray.shift();
var hexExpected = "331200001d02e467b9dd11fa00df2c0000801b020080010300800000000000000000";
common.compare(hexOutgoing, hexExpected, "apdu command", {cla:1, ins:1, p1:1, p2:1, len:1, slot:1, slotBytes:28, unexpected:9999});
common.testStep(" >    ", "APDU out");
common.asyncCurlApduSend(hexOutgoing);
common.testStep("   +  ", "buttons");
common.curlScreenShot(scriptName); common.curlButton('right', "; navigate the address / path; Set Account 2");
common.curlScreenShot(scriptName); common.curlButton('right', "; navigate the address / path; Account e467..");
common.curlScreenShot(scriptName); common.curlButton('right', "; navigate the address / path; Path 44'/..");
common.curlScreenShot(scriptName); common.curlButton('both', "; confirm; Approve");
common.curlScreenShot(scriptName); console.log(common.humanTime() + " // back to main screen");
common.testStep("     <", "APDU in");
var hexResponse = await common.curlApduResponseWait();
var hexExpected = "9000";
common.compare(hexResponse, hexExpected, "apdu response", {returnCode:2, unexpected:9999});

common.testStep(" - - -", "await app.getAddressAndPubKey() // slot=" + slot);
await app.getAddressAndPubKey(slot);
var hexOutgoing = common.hexApduCommandViaMockTransportArray.shift();
var hexExpected = "330100000102";
common.compare(hexOutgoing, hexExpected, "apdu command", {cla:1, ins:1, p1:1, p2:1, len:1, slot:1, unexpected:9999});
common.testStep(" >    ", "APDU out");
common.asyncCurlApduSend(hexExpected);
common.testStep("     <", "APDU in");
var hexResponse = await common.curlApduResponseWait();
var hexEXpected = "e467b9dd11fa00df04db0a14364e5bf43a7ddda603522ddfee95c5ff12b48c49480f062e7aa9d20e84215eef9b8b76175f32802f75ed54110e29c7dc76054f24c028c312098e7177a39000";
common.compare(hexResponse, hexEXpected, "apdu response", {address:8, publicKey:65, returnCode:2, unexpected:9999});

common.testEnd(scriptName);

// Above is the speculos-only / zemu-free test.
// Below is the original zemu test for comparison:
/*
    test("get address - secp256r1", async function () {
        const sim = new Zemu(APP_PATH);
        try {
            await sim.start(simOptions);
            const app = new FlowApp(sim.getTransport());

            const scheme = FlowApp.Signature.P256 | FlowApp.Hash.SHA2_256;
            const path = `m/44'/539'/${scheme}'/0/0`;
            const address = "e467b9dd11fa00df"

            await prepareSlot(sim, app, 0, address, path)

            const resp = await app.getAddressAndPubKey(0);

            expect(resp.returnCode).toEqual(0x9000);
            expect(resp.errorMessage).toEqual("No errors");

            const expected_address_string = "e467b9dd11fa00df";
            const expected_pk = "04db0a14364e5bf43a7ddda603522ddfee95c5ff12b48c49480f062e7aa9d20e84215eef9b8b76175f32802f75ed54110e29c7dc76054f24c028c312098e7177a3";

            expect(resp.address).toEqual(expected_address_string);
            expect(resp.publicKey.toString('hex')).toEqual(expected_pk);

        } finally {
            await sim.close();
        }
    });
*/
