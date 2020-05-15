import pymongo
import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots

myclient = pymongo.MongoClient("mongodb://localhost:27017/")
mydb = myclient["test"]
mycol = mydb["log"]

df = pd.DataFrame(list(mycol.find()),columns=["datetime",
                                              "tempSP","tempErr","tempPV","tempExt",
                                              "relhumSP","relhumErr","relhumPV","relhumExt"])

df['tempSP'] = df['tempSP'].astype(float)
df['tempErr'] = df['tempErr'].astype(float)
df['tempSPMinErr'] = df.tempSP + df.tempErr
df['tempSPMaxErr'] = df.tempSP - df.tempErr

df['relhumSP'] = df['relhumSP'].astype(float)
df['relhumErr'] = df['relhumErr'].astype(float)
df['relhumSPMinErr'] = df.relhumSP + df.relhumErr
df['relhumSPMaxErr'] = df.relhumSP - df.relhumErr

fig = go.Figure()

fig = make_subplots(rows=2, cols=1)

## temperature
fig.add_trace(go.Scatter(x=df.datetime, y=df.tempSP,
                    mode='lines',
                    name='tempSP'),
              row=1, col=1)
fig.add_trace(go.Scatter(x=df.datetime, y=df.tempSPMaxErr,
                    mode='lines',
                    name='tempSPMaxErr'),
              row=1, col=1)
fig.add_trace(go.Scatter(x=df.datetime, y=df.tempSPMinErr,
                    mode='lines',
                    name='tempSPMinErr'),
              row=1, col=1)
fig.add_trace(go.Scatter(x=df.datetime, y=df.tempPV,
                    mode='lines',
                    name='tempPV'),
              row=1, col=1)
fig.add_trace(go.Scatter(x=df.datetime, y=df.tempExt,
                    mode='lines',
                    name='tempExt'),
              row=1, col=1)

## relative humidity
fig.add_trace(go.Scatter(x=df.datetime, y=df.relhumSP,
                    mode='lines',
                    name='relhumSP'),
              row=2, col=1)
fig.add_trace(go.Scatter(x=df.datetime, y=df.relhumSPMaxErr,
                    mode='lines',
                    name='relhumSPMaxErr'),
              row=2, col=1)
fig.add_trace(go.Scatter(x=df.datetime, y=df.relhumSPMinErr,
                    mode='lines',
                    name='relhumSPMinErr'),
              row=2, col=1)
fig.add_trace(go.Scatter(x=df.datetime, y=df.relhumPV,
                    mode='lines',
                    name='relhumPV'),
              row=2, col=1)
fig.add_trace(go.Scatter(x=df.datetime, y=df.relhumExt,
                    mode='lines',
                    name='relhumExt'),
              row=2, col=1)

fig.update_xaxes(title_text="DateTime (min)", row=1, col=1)
fig.update_yaxes(title_text="Temperature (°C)", row=1, col=1)
fig.update_xaxes(title_text="DateTime (min)", row=2, col=1)
fig.update_yaxes(title_text="Relative Humidity (%)", row=2, col=1)

fig.update_layout(
    title="Temperature and Relative Humidity",
    font=dict(
        family="Arial",
        size=15,
        color="#fff"
    ),
    paper_bgcolor="#000",
    plot_bgcolor="#000",
)

fig.show()
