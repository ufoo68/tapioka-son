'use strict';

const express = require('express');
const line = require('@line/bot-sdk');
const fetch = require('node-fetch');
require('dotenv').config();

const PORT = process.env.PORT || 3000;

// channel secretとaccess tokenを.envの環境変数から呼び出す
const config = {
    channelSecret: process.env.CHANNEL_SECRET,
    channelAccessToken: process.env.ACCESS_TOKEN
};

function buildReplyText(text) {
  return {
    type: 'text',
    text: text
  }
}
const app = express();
//URL + /webhookで登録したWebhook URLが呼び出されたときに実行される。
app.post('/webhook', line.middleware(config), (req, res) => {
    Promise
      .all(req.body.events.map(handleEvent))
      .then((result) => res.json(result));
});

const client = new line.Client(config);
//ユーザから受け取ったイベントについてのハンドリングを実装する
async function handleEvent(event) {
  switch (event.type) {
    case 'things':
      const thingsData = event.things.result;    
      const blePayload = thingsData.bleNotificationPayload;
      const buffer = new Buffer.from(blePayload, 'base64');
      const data = parseInt(buffer.toString('hex'), 16);
      console.log("Payload=" + data);
      fetch(`https://f2ea0e8e.ngrok.io/postTapioka`, {
        method: "POST",
        headers: {
          'Accept': 'application/json',
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({step: data})
      })
      return client.replyMessage(event.replyToken, buildReplyText('今日の歩数をカウントしたよ'));
    case 'message':
      if (event.message.text == '今日のタピオカ') {
        const res = await fetch(`https://f2ea0e8e.ngrok.io/getTapioka`);
        const data = await res.json();
        console.log(data)
        if(data.step > 200) {
          return client.replyMessage(event.replyToken, buildReplyText('タピオカバナナミルクを飲みましょう'));
        } else if(data.step > 150) {
          return client.replyMessage(event.replyToken, buildReplyText('タピオカイチゴミルクを飲みましょう'));
        } else if(data.step > 100) {
          return client.replyMessage(event.replyToken, buildReplyText('タピオカミルクティーを飲みましょう'));
        }
        return client.replyMessage(event.replyToken, buildReplyText('麦茶を飲みましょう'));
      }
    default:
      return Promise.resolve(null);
  }
}

app.listen(PORT);
console.log(`Server running at ${PORT}`);