import { Badge } from "@/components/ui/badge"
import { cn } from "@/lib/utils"
import type { ConnectionStatus } from "@/lib/types"

const LABEL: Record<ConnectionStatus, string> = {
  open: "Live",
  connecting: "Connecting",
  stalled: "Reconnecting",
  closed: "Offline",
}

export function ConnectionBadge({
  status,
  demo,
}: {
  status: ConnectionStatus
  demo: boolean
}) {
  const live = status === "open"
  return (
    <Badge variant="outline" className="gap-1.5 pl-2">
      <span
        className={cn(
          "size-1.5 rounded-full",
          live ? "animate-breathe bg-ok" : "bg-warn",
          status === "closed" && "bg-destructive"
        )}
      />
      {demo ? "Demo" : LABEL[status]}
    </Badge>
  )
}
