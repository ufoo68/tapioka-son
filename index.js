'use strict';

const express = require('express');
const line = require('@line/bot-sdk');
require('dotenv').config();

const PORT = process.env.PORT || 3000;

// channel secretとaccess tokenを.envの環境変数から呼び出す
const config = {
    channelSecret: process.env.CHANNEL_SECRET,
    channelAccessToken: process.env.ACCESS_TOKEN
};

const app = express();
//URL + /webhookで登録したWebhook URLが呼び出されたときに実行される。
app.post('/webhook', line.middleware(config), (req, res) => {
    Promise
      .all(req.body.events.map(handleEvent))
      .then((result) => res.json(result));
});

const client = new line.Client(config);
//ユーザから受け取ったイベントについてのハンドリングを実装する
function handleEvent(event) {
    let mes = '';
    if (event.type !== 'things') {
      return Promise.resolve(null);
    }
  
    if(event.type === 'things' && event.things.type === 'link'){
      mes = 'デバイスと接続しました。';
    }else if(event.type === 'things' && event.things.type === 'unlink'){
      mes = 'デバイスとの接続を解除しました。';
    }else{
      const thingsData = event.things.result;    
      if (!thingsData.bleNotificationPayload) return
      // bleNotificationPayloadにデータが来る
      const blePayload = thingsData.bleNotificationPayload;
      const buffer = new Buffer.from(blePayload, 'base64');
      const data = buffer.toString('hex');  //Base64をデコード
      console.log("Payload=" + parseInt(data,16));
      mes = `${parseInt(data,16)}歩あるいたよ`;
      const msgObj = {
        type: 'text',
        text: mes //実際に返信の言葉を入れる箇所
      }
  
      return client.replyMessage(event.replyToken, msgObj);
    }
  }

app.listen(PORT);
console.log(`Server running at ${PORT}`);