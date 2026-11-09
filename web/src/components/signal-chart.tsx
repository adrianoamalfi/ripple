import { useMemo, useState } from "react"
import {
  Area,
  CartesianGrid,
  ComposedChart,
  Line,
  ReferenceArea,
  XAxis,
  YAxis,
} from "recharts"

import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/ui/card"
import {
  ChartContainer,
  ChartTooltip,
  ChartTooltipContent,
  type ChartConfig,
} from "@/components/ui/chart"
import { Skeleton } from "@/components/ui/skeleton"
import { ToggleGroup, ToggleGroupItem } from "@/components/ui/toggle-group"
import { motionRanges, toChartPoints, type Bucket } from "@/lib/history"

const config = {
  peak: { label: "Fluctuation", color: "var(--chart-1)" },
  thr: { label: "Threshold", color: "var(--chart-2)" },
} satisfies ChartConfig

const WINDOWS = [
  { value: "60", label: "1 min", seconds: 60, tickEvery: 10 },
  { value: "300", label: "5 min", seconds: 300, tickEvery: 60 },
  { value: "900", label: "15 min", seconds: 900, tickEvery: 180 },
] as const

function formatAge(x: number, long: boolean): string {
  if (x === 0) return "now"
  const s = -x
  if (long && s >= 120) return `${Math.round(s / 60)}m`
  return `${s}s`
}

export function SignalChart({
  getHistory,
  nowMs,
}: {
  getHistory: () => Bucket[]
  nowMs: number
}) {
  const [windowKey, setWindowKey] = useState<string>("60")
  const win = WINDOWS.find((w) => w.value === windowKey) ?? WINDOWS[0]

  // Rebuilt once a second (nowMs changes), not on every 5 Hz message: keeps
  // the chart cheap on phones.
  const points = useMemo(
    () => toChartPoints(getHistory(), win.seconds, nowMs),
    [getHistory, win.seconds, nowMs]
  )
  const ranges = useMemo(() => motionRanges(points), [points])
  const hasData = points.some((p) => p.peak !== null)

  const ticks = useMemo(() => {
    const out: number[] = []
    for (let x = -win.seconds; x <= 0; x += win.tickEvery) out.push(x)
    return out
  }, [win])

  return (
    <Card>
      {/* Controls sit below the text on phones, beside it from `sm` up. */}
      <CardHeader className="flex flex-col gap-3 sm:flex-row sm:items-start sm:justify-between">
        <div className="flex flex-col gap-1">
          <CardTitle>Signal fluctuation</CardTitle>
          <CardDescription>
            How much the WiFi signal is moving. Motion starts above the dashed
            threshold.
          </CardDescription>
        </div>
        <ToggleGroup
          type="single"
          variant="outline"
          size="sm"
          value={windowKey}
          onValueChange={(v) => v && setWindowKey(v)}
          aria-label="Chart time range"
        >
          {WINDOWS.map((w) => (
            <ToggleGroupItem key={w.value} value={w.value}>
              {w.label}
            </ToggleGroupItem>
          ))}
        </ToggleGroup>
      </CardHeader>
      <CardContent>
        {!hasData ? (
          <Skeleton className="h-64 w-full" />
        ) : (
          <ChartContainer config={config} className="aspect-auto h-64 w-full">
            <ComposedChart
              data={points}
              margin={{ top: 8, right: 8, bottom: 0, left: 0 }}
            >
              <defs>
                <linearGradient id="fillPeak" x1="0" y1="0" x2="0" y2="1">
                  <stop offset="0%" stopColor="var(--color-peak)" stopOpacity={0.35} />
                  <stop offset="100%" stopColor="var(--color-peak)" stopOpacity={0.02} />
                </linearGradient>
              </defs>
              <CartesianGrid vertical={false} strokeDasharray="3 3" />
              {ranges.map(([a, b]) => (
                <ReferenceArea
                  key={a}
                  x1={a}
                  x2={b}
                  fill="var(--chart-4)"
                  fillOpacity={0.16}
                  ifOverflow="hidden"
                />
              ))}
              <XAxis
                dataKey="x"
                type="number"
                domain={[-win.seconds, 0]}
                ticks={ticks}
                tickFormatter={(x: number) => formatAge(x, win.seconds > 60)}
                tickLine={false}
                axisLine={false}
                tickMargin={8}
              />
              <YAxis
                width={32}
                tickLine={false}
                axisLine={false}
                allowDecimals={false}
                domain={[0, (max: number) => Math.max(4, Math.ceil(max * 1.15))]}
              />
              <ChartTooltip
                cursor={{ stroke: "var(--border)" }}
                content={
                  <ChartTooltipContent
                    indicator="line"
                    labelFormatter={(_, payload) => {
                      const x = payload?.[0]?.payload?.x as number | undefined
                      return x === undefined ? "" : x === 0 ? "Now" : `${-x}s ago`
                    }}
                    formatter={(value, name) => (
                      <div className="flex w-full items-center justify-between gap-4">
                        <span className="text-muted-foreground">
                          {config[name as keyof typeof config]?.label ?? name}
                        </span>
                        <span className="tabular font-mono font-medium">
                          {typeof value === "number" ? value.toFixed(2) : value}
                        </span>
                      </div>
                    )}
                  />
                }
              />
              <Area
                dataKey="peak"
                type="monotone"
                stroke="var(--color-peak)"
                strokeWidth={2}
                fill="url(#fillPeak)"
                dot={false}
                isAnimationActive={false}
                connectNulls={false}
              />
              <Line
                dataKey="thr"
                type="stepAfter"
                stroke="var(--color-thr)"
                strokeWidth={1.5}
                strokeDasharray="6 4"
                dot={false}
                isAnimationActive={false}
                connectNulls={false}
              />
            </ComposedChart>
          </ChartContainer>
        )}
      </CardContent>
    </Card>
  )
}
